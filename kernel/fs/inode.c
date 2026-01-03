#include "inode.h"
#include "buf.h"
#include "fs.h"
#include "bitmap.h"
#include "lib/print.h"
#include "mem/str.h"

// inode缓存
static struct inode icache[NINODE];
static struct spinlock lk_icache;

// 每个block包含的inode数
#define IPB (1024 / sizeof(struct dinode))

// 获取inode所在的block号
static uint32 inode_block(uint32 inum) {
    return sb.inodestart + (inum - 1) / IPB;
}

// 获取inode在block内的偏移
static uint32 inode_offset(uint32 inum) {
    return (inum - 1) % IPB;
}

// 初始化inode缓存
void inode_init(void) {
    spinlock_init(&lk_icache, "icache");
    for (int i = 0; i < NINODE; i++) {
        spinlock_init(&icache[i].lock, "inode");
        icache[i].ref = 0;
        icache[i].valid = 0;
    }
}

// 从磁盘读取inode
static void inode_read(struct inode* ip) {
    struct buf *b = buf_read(inode_block(ip->inum));
    if (b == NULL) {
        panic("inode_read: failed to read inode block");
    }
    
    struct dinode *dip = (struct dinode*)b->data + inode_offset(ip->inum);
    memcpy(&ip->dinode, dip, sizeof(struct dinode));
    
    buf_release(b);
    ip->valid = 1;
}

// 将inode写回磁盘
static void inode_write(struct inode* ip) {
    struct buf *b = buf_read(inode_block(ip->inum));
    if (b == NULL) {
        panic("inode_write: failed to read inode block");
    }
    
    struct dinode *dip = (struct dinode*)b->data + inode_offset(ip->inum);
    memcpy(dip, &ip->dinode, sizeof(struct dinode));
    
    buf_write(b);
    buf_release(b);
}

// 获取inode（增加引用计数）
struct inode* inode_get(uint32 dev, uint32 inum) {
    struct inode *ip, *empty = NULL;
    
    spinlock_acquire(&lk_icache);
    
    // 查找缓存中是否已有
    for (ip = &icache[0]; ip < &icache[NINODE]; ip++) {
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
            ip->ref++;
            spinlock_release(&lk_icache);
            return ip;
        }
        if (empty == NULL && ip->ref == 0) {
            empty = ip;
        }
    }
    
    // 未命中，分配一个空闲inode缓存
    if (empty == NULL) {
        panic("inode_get: no free inode cache");
    }
    
    ip = empty;
    ip->dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;
    
    spinlock_release(&lk_icache);
    
    // 从磁盘读取
    inode_read(ip);
    
    return ip;
}

// 释放inode（减少引用计数）
void inode_put(struct inode* ip) {
    if (ip == NULL) {
        return;
    }
    
    spinlock_acquire(&lk_icache);
    
    if (ip->ref < 1) {
        panic("inode_put: ref < 1");
    }
    
    ip->ref--;
    
    if (ip->ref == 0) {
        // 写回磁盘
        inode_write(ip);
        ip->valid = 0;
    }
    
    spinlock_release(&lk_icache);
}

// 分配新inode
struct inode* inode_alloc(uint32 dev, uint16 type) {
    uint32 inum = ialloc();
    if (inum == 0) {
        return NULL;
    }
    
    struct inode *ip = inode_get(dev, inum);
    if (ip == NULL) {
        ifree(inum);
        return NULL;
    }
    
    inode_lock(ip);
    ip->dinode.type = type;
    ip->dinode.major = 0;
    ip->dinode.minor = 0;
    ip->dinode.nlink = 1;
    ip->dinode.size = 0;
    memset(ip->dinode.addrs, 0, sizeof(ip->dinode.addrs));
    inode_update(ip);
    inode_unlock(ip);
    
    return ip;
}

// 锁定inode
void inode_lock(struct inode* ip) {
    spinlock_acquire(&ip->lock);
}

// 解锁inode
void inode_unlock(struct inode* ip) {
    spinlock_release(&ip->lock);
}

// 更新inode到磁盘
void inode_update(struct inode* ip) {
    // inode_write会在inode_put时调用，这里可以立即写回
    struct buf *b = buf_read(inode_block(ip->inum));
    if (b == NULL) {
        panic("inode_update: failed to read inode block");
    }
    
    struct dinode *dip = (struct dinode*)b->data + inode_offset(ip->inum);
    memcpy(dip, &ip->dinode, sizeof(struct dinode));
    
    buf_write(b);
    buf_release(b);
}

// 截断inode（释放所有数据块）
void inode_truncate(struct inode* ip) {
    // 释放所有直接块
    for (int i = 0; i < 10; i++) {
        if (ip->dinode.addrs[i]) {
            bfree(ip->dinode.addrs[i]);
            ip->dinode.addrs[i] = 0;
        }
    }
    
    // 释放一级间接块
    if (ip->dinode.addrs[10]) {
        struct buf *b = buf_read(ip->dinode.addrs[10]);
        uint32 *addrs = (uint32*)b->data;
        for (int i = 0; i < 256; i++) {  // 每个block有256个uint32
            if (addrs[i]) {
                bfree(addrs[i]);
            }
        }
        buf_release(b);
        bfree(ip->dinode.addrs[10]);
        ip->dinode.addrs[10] = 0;
    }
    
    // 释放二级间接块（简化实现，暂不处理）
    
    ip->dinode.size = 0;
    inode_update(ip);
}

// 释放inode
void inode_free(struct inode* ip) {
    if (ip->dinode.nlink == 0) {
        inode_truncate(ip);
        ifree(ip->inum);
    }
    ip->dinode.nlink--;
    inode_update(ip);
}

// 定位第bn个逻辑块对应的物理块号
uint32 inode_locate_block(struct inode* ip, uint32 bn) {
    if (bn < 10) {
        // 直接索引
        if (ip->dinode.addrs[bn] == 0) {
            ip->dinode.addrs[bn] = balloc();
            inode_update(ip);
        }
        return ip->dinode.addrs[bn];
    }
    
    bn -= 10;
    if (bn < 256) {
        // 一级间接索引
        if (ip->dinode.addrs[10] == 0) {
            ip->dinode.addrs[10] = balloc();
            inode_update(ip);
        }
        struct buf *b = buf_read(ip->dinode.addrs[10]);
        uint32 *addrs = (uint32*)b->data;
        if (addrs[bn] == 0) {
            addrs[bn] = balloc();
            buf_write(b);
        }
        uint32 addr = addrs[bn];
        buf_release(b);
        return addr;
    }
    
    bn -= 256;
    // 二级间接索引（简化实现，暂不支持）
    panic("inode_locate_block: bn too large");
    return 0;
}

// 从inode读取数据
int inode_read_data(struct inode* ip, uint32 off, void* dst, uint32 n) {
    if (off + n > ip->dinode.size) {
        n = ip->dinode.size - off;
    }
    
    uint32 tot = 0;
    while (tot < n) {
        uint32 bn = (off + tot) / 1024;
        uint32 boff = (off + tot) % 1024;
        uint32 m = 1024 - boff;
        if (m > n - tot) {
            m = n - tot;
        }
        
        uint32 blockno = inode_locate_block(ip, bn);
        if (blockno == 0) {
            break;
        }
        
        struct buf *b = buf_read(blockno);
        memcpy((char*)dst + tot, b->data + boff, m);
        buf_release(b);
        
        tot += m;
    }
    
    return tot;
}

// 向inode写入数据
int inode_write_data(struct inode* ip, uint32 off, const void* src, uint32 n) {
    uint32 tot = 0;
    while (tot < n) {
        uint32 bn = (off + tot) / 1024;
        uint32 boff = (off + tot) % 1024;
        uint32 m = 1024 - boff;
        if (m > n - tot) {
            m = n - tot;
        }
        
        uint32 blockno = inode_locate_block(ip, bn);
        if (blockno == 0) {
            break;
        }
        
        struct buf *b = buf_read(blockno);
        memcpy(b->data + boff, (char*)src + tot, m);
        buf_write(b);
        buf_release(b);
        
        tot += m;
    }
    
    if (off + tot > ip->dinode.size) {
        ip->dinode.size = off + tot;
        inode_update(ip);
    }
    
    return tot;
}

// 打印inode信息（调试用）
void inode_print(struct inode* ip) {
    printf("inode %d: type=%d, size=%d, nlink=%d\n", 
           ip->inum, ip->dinode.type, ip->dinode.size, ip->dinode.nlink);
    printf("  addrs: ");
    for (int i = 0; i < 12; i++) {
        if (ip->dinode.addrs[i]) {
            printf("%d ", ip->dinode.addrs[i]);
        }
    }
    printf("\n");
}

