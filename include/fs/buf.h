#ifndef __BUF_H__
#define __BUF_H__

#include "common.h"
#include "lib/lock.h"

// 块缓存数量
#define N_BLOCK_BUF 6

// 块缓存结构
struct buf {
    int valid;          // 数据是否有效
    int disk;           // 是否在磁盘上（用于写回）
    uint32 blockno;     // 块号
    uint8 data[1024];   // 块数据（BLOCK_SIZE = 1024）
    int ref;            // 引用计数
    struct buf *prev;   // 双向链表前驱
    struct buf *next;   // 双向链表后继
};

// 块缓存初始化
void buf_init(void);

// 读取块（返回持有引用的buf）
struct buf* buf_read(uint32 blockno);

// 标记buf为dirty（需要写回）
void buf_write(struct buf* b);

// 释放buf引用
void buf_release(struct buf* b);

// 打印buf缓存状态（调试用）
void show_buf(void);

#endif

