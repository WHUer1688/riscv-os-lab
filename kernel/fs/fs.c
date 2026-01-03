#include "fs.h"
#include "buf.h"
#include "inode.h"
#include "lib/print.h"
#include "mem/str.h"

// ===== inode rw self-test helpers =====
#ifndef BLOCK_SIZE
#define BLOCK_SIZE 1024
#endif

static unsigned char str[2 * BLOCK_SIZE];
static unsigned char tmp[2 * BLOCK_SIZE];

static int blockcmp(const void *a, const void *b) {
    const unsigned char *pa = (const unsigned char*)a;
    const unsigned char *pb = (const unsigned char*)b;
    for (int i = 0; i < 2 * BLOCK_SIZE; i++) {
        if (pa[i] != pb[i]) return 0;
    }
    return 1;
}

#ifndef assert
#define assert(cond, msg) do { \
  if(!(cond)) { printf("ASSERT FAIL: %s\n", msg); while(1){} } \
} while(0)
#endif
// ===== end helpers =====

// 全局super block
struct super_block sb;

// 文件系统初始化
void fs_init(void) {
    // 读取super block
    struct buf *b = buf_read(SB_BLOCK);
    if (b == NULL) {
        panic("fs_init: failed to read super block");
    }
    
    // 复制到内存
    memcpy(&sb, b->data, sizeof(struct super_block));
    
    // 检查魔数（可以自定义，比如0x12345678）
    if (sb.magic != 0x12345678) {
        panic("fs_init: invalid magic");
    }
    
    // 释放buf
    buf_release(b);
    
    // 初始化inode缓存
    inode_init();
    
    // ===== inode rw self-test =====
    int ret = 0;
    
    for(int i = 0; i < BLOCK_SIZE * 2; i++)
        str[i] = (unsigned char)i;
    
    // 创建新的inode
    struct inode* nip = inode_alloc(1, T_FILE);  // dev=1, type=T_FILE
    assert(nip != 0, "inode_create: returned NULL");
    
    inode_lock(nip);
    
    // 第一次查看（可选）
    inode_print(nip);
    
    // 第一次写入：从偏移0写入 BLOCK_SIZE/2 字节
    ret = inode_write_data(nip, 0, str, BLOCK_SIZE / 2);
    assert(ret == BLOCK_SIZE / 2, "inode_write_data: fail (1)");
    
    // 第二次写入：从偏移 BLOCK_SIZE/2 写入 BLOCK_SIZE + BLOCK_SIZE/2 字节
    ret = inode_write_data(nip, BLOCK_SIZE / 2, str + BLOCK_SIZE / 2, BLOCK_SIZE + BLOCK_SIZE / 2);
    assert(ret == BLOCK_SIZE + BLOCK_SIZE / 2, "inode_write_data: fail (2)");
    
    // 一次读取：从偏移0读取 BLOCK_SIZE * 2 字节
    ret = inode_read_data(nip, 0, tmp, BLOCK_SIZE * 2);
    assert(ret == BLOCK_SIZE * 2, "inode_read_data: fail");
    
    // 第二次查看（可选）
    inode_print(nip);
    
    inode_unlock(nip);
    inode_put(nip);
    
    // 测试结果
    if(blockcmp(tmp, str) == 1)
        printf("success\n");
    else
        printf("fail\n");
    
    while (1);
    // ===== end self-test =====
    
    // 初始化文件系统
    extern void file_init(void);
    file_init();
    
    printf("fs_init: file system initialized\n");
    printf("  magic: 0x%x\n", sb.magic);
    printf("  size: %d blocks\n", sb.size);
    printf("  nblocks: %d\n", sb.nblocks);
    printf("  ninodes: %d\n", sb.ninodes);
    printf("  inodestart: %d\n", sb.inodestart);
    printf("  bmapstart: %d\n", sb.bmapstart);
    printf("  datastart: %d\n", sb.datastart);
}

