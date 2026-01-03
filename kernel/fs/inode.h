#ifndef __INODE_H__
#define __INODE_H__

#include "common.h"
#include "lib/lock.h"

// 文件类型
#define T_DIR  1   // 目录
#define T_FILE 2   // 文件
#define T_DEV  3   // 设备

// inode结构（磁盘上）
struct dinode {
    uint16 type;           // 文件类型
    uint16 major;          // 主设备号（T_DEV）
    uint16 minor;          // 次设备号（T_DEV）
    uint16 nlink;          // 链接数
    uint32 size;           // 文件大小（字节）
    uint32 addrs[12];      // 数据块地址（直接索引：10个，一级间接：1个，二级间接：1个）
};

// inode缓存结构（内存中）
struct inode {
    uint32 dev;            // 设备号
    uint32 inum;           // inode号
    int ref;               // 引用计数
    struct spinlock lock;  // 保护inode的锁
    int valid;             // inode是否从磁盘读取
    struct dinode dinode;  // 磁盘inode的副本
};

// inode缓存大小
#define NINODE 50

// inode操作
void inode_init(void);
struct inode* inode_get(uint32 dev, uint32 inum);
void inode_put(struct inode* ip);
struct inode* inode_alloc(uint32 dev, uint16 type);
void inode_lock(struct inode* ip);
void inode_unlock(struct inode* ip);
void inode_update(struct inode* ip);
void inode_truncate(struct inode* ip);
void inode_free(struct inode* ip);

// 数据操作
uint32 inode_locate_block(struct inode* ip, uint32 bn);
int inode_read_data(struct inode* ip, uint32 off, void* dst, uint32 n);
int inode_write_data(struct inode* ip, uint32 off, const void* src, uint32 n);

// 调试
void inode_print(struct inode* ip);

#endif

