#include "buf.h"
#include "dev/virtio.h"
#include "lib/print.h"
#include "mem/str.h"

// 全局buf缓存数组
static struct buf bufs[N_BLOCK_BUF];
static spinlock_t lk_buf_cache;

// 双向循环链表头
// head_buf->next: 已分配链（有ref的buf）
// head_buf->prev: 可分配链（ref==0的buf，LRU顺序）
static struct buf head_buf;

// 初始化buf缓存
void buf_init(void) {
    spinlock_init(&lk_buf_cache, "buf_cache");
    
    // 初始化head_buf（双向循环链表）
    head_buf.prev = &head_buf;
    head_buf.next = &head_buf;
    
    // 初始化所有buf，全部加入可分配链（prev端）
    for (int i = 0; i < N_BLOCK_BUF; i++) {
        struct buf *b = &bufs[i];
        b->valid = 0;
        b->disk = 0;
        b->blockno = 0;
        b->ref = 0;
        memset(b->data, 0, 1024);
        
        // 插入到可分配链（head_buf.prev端，LRU顺序）
        b->next = &head_buf;
        b->prev = head_buf.prev;
        head_buf.prev->next = b;
        head_buf.prev = b;
    }
}

// 从磁盘读取块到buf
static void buf_load(struct buf *b) {
    if (virtio_disk_rw(b->blockno, b->data, 0) < 0) {
        panic("buf_load: disk read failed");
    }
    b->valid = 1;
    b->disk = 1;
}

// 将buf写回磁盘
static void buf_writeback(struct buf *b) {
    if (!b->valid || !b->disk) {
        return;
    }
    if (virtio_disk_rw(b->blockno, b->data, 1) < 0) {
        panic("buf_writeback: disk write failed");
    }
    b->disk = 0;  // 清除dirty标记
}

// 从链表中移除buf
static void buf_remove(struct buf *b) {
    b->next->prev = b->prev;
    b->prev->next = b->next;
}

// 将buf插入到已分配链（next端，最近使用端）
static void buf_insert_allocated(struct buf *b) {
    b->next = head_buf.next;
    b->prev = &head_buf;
    head_buf.next->prev = b;
    head_buf.next = b;
}

// 将buf插入到可分配链（prev端，LRU顺序，最久未使用在最前）
static void buf_insert_free(struct buf *b) {
    b->next = &head_buf;
    b->prev = head_buf.prev;
    head_buf.prev->next = b;
    head_buf.prev = b;
}

// 读取块（返回持有引用的buf）
struct buf* buf_read(uint32 blockno) {
    spinlock_acquire(&lk_buf_cache);
    
    // 1. 查找是否已在缓存中（在已分配链中查找）
    struct buf *b;
    for (b = head_buf.next; b != &head_buf; b = b->next) {
        if (b->ref > 0 && b->blockno == blockno && b->valid) {
            // 命中缓存，增加引用计数
            b->ref++;
            // 移动到最近使用端（LRU策略）
            buf_remove(b);
            buf_insert_allocated(b);
            spinlock_release(&lk_buf_cache);
            return b;
        }
    }
    
    // 2. 未命中，从可分配链获取一个buf（LRU：最久未使用的）
    if (head_buf.prev == &head_buf) {
        panic("buf_read: no free buf");
    }
    b = head_buf.prev;  // 最久未使用的buf
    
    // 如果buf有效且dirty，先写回
    if (b->valid && b->disk) {
        buf_writeback(b);
    }
    
    // 从链表中移除
    buf_remove(b);
    
    // 设置新的块号
    b->blockno = blockno;
    b->valid = 0;
    b->disk = 0;
    
    // 从磁盘读取
    buf_load(b);
    
    // 设置引用计数并插入已分配链
    b->ref = 1;
    buf_insert_allocated(b);
    
    spinlock_release(&lk_buf_cache);
    return b;
}

// 标记buf为dirty（需要写回）
void buf_write(struct buf* b) {
    if (b == NULL) {
        return;
    }
    spinlock_acquire(&lk_buf_cache);
    b->disk = 1;  // 标记为dirty（懒惰写回：不立即写磁盘）
    spinlock_release(&lk_buf_cache);
}

// 释放buf引用
void buf_release(struct buf* b) {
    if (b == NULL) {
        return;
    }
    spinlock_acquire(&lk_buf_cache);
    
    if (b->ref <= 0) {
        panic("buf_release: ref <= 0");
    }
    
    b->ref--;
    
    if (b->ref == 0) {
        // 引用计数为0，移动到可分配链（LRU顺序）
        buf_remove(b);
        buf_insert_free(b);
    }
    
    spinlock_release(&lk_buf_cache);
}

// 打印buf缓存状态（调试用）
void show_buf(void) {
    spinlock_acquire(&lk_buf_cache);
    
    printf("=== Buf Cache State ===\n");
    printf("Allocated chain (head->next): ");
    struct buf *b;
    int count = 0;
    for (b = head_buf.next; b != &head_buf; b = b->next) {
        if (b->ref > 0) {
            printf("[blockno=%d,ref=%d] ", b->blockno, b->ref);
            count++;
        }
    }
    printf("\n");
    
    printf("Free chain (head->prev): ");
    count = 0;
    for (b = head_buf.prev; b != &head_buf; b = b->prev) {
        if (b->ref == 0) {
            printf("[blockno=%d,valid=%d] ", b->blockno, b->valid);
            count++;
        }
    }
    printf("\n");
    
    spinlock_release(&lk_buf_cache);
}

