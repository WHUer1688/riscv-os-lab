#ifndef __FILE_H__
#define __FILE_H__

#include "common.h"
#include "inode.h"

// 文件类型
#define FD_NONE   0
#define FD_PIPE   1
#define FD_INODE  2
#define FD_DEVICE 3

// 文件结构
struct file {
    int type;           // 文件类型
    int ref;            // 引用计数
    char readable;      // 可读
    char writable;      // 可写
    struct inode *ip;   // inode指针（FD_INODE）
    uint32 off;         // 文件偏移
};

// 最大文件描述符数
#define NFILE 16

// 文件操作
struct file* file_alloc(void);
struct file* file_dup(struct file* f);
void file_close(struct file* f);
struct file* file_open(const char* path, int omode);
int file_read(struct file* f, void* dst, int n);
int file_write(struct file* f, const void* src, int n);

#endif

