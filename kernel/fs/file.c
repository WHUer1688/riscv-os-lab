#include "file.h"
#include "inode.h"
#include "dir.h"
#include "proc/proc.h"
#include "mem/kvm.h"
#include "lib/print.h"
#include "mem/str.h"

// 全局文件数组
static struct file files[NFILE];
static struct spinlock lk_file;

// 初始化文件系统
void file_init(void) {
    spinlock_init(&lk_file, "file");
    for (int i = 0; i < NFILE; i++) {
        files[i].type = FD_NONE;
        files[i].ref = 0;
    }
}

// 分配文件结构
struct file* file_alloc(void) {
    spinlock_acquire(&lk_file);
    for (int i = 0; i < NFILE; i++) {
        if (files[i].ref == 0) {
            files[i].ref = 1;
            files[i].type = FD_NONE;
            files[i].readable = 0;
            files[i].writable = 0;
            files[i].ip = NULL;
            files[i].off = 0;
            spinlock_release(&lk_file);
            return &files[i];
        }
    }
    spinlock_release(&lk_file);
    return NULL;
}

// 复制文件结构（增加引用计数）
struct file* file_dup(struct file* f) {
    if (f == NULL) {
        return NULL;
    }
    spinlock_acquire(&lk_file);
    f->ref++;
    spinlock_release(&lk_file);
    return f;
}

// 关闭文件（减少引用计数）
void file_close(struct file* f) {
    if (f == NULL) {
        return;
    }
    spinlock_acquire(&lk_file);
    if (f->ref < 1) {
        panic("file_close: ref < 1");
    }
    f->ref--;
    if (f->ref == 0) {
        if (f->type == FD_INODE && f->ip) {
            inode_put(f->ip);
            f->ip = NULL;
        }
        f->type = FD_NONE;
    }
    spinlock_release(&lk_file);
}

// 打开文件
struct file* file_open(const char* path, int omode) {
    struct inode *ip;
    
    // 解析路径
    ip = path_to_inode(path);
    if (ip == NULL) {
        // 文件不存在，如果是要创建文件
        if (omode & 0x200) {  // O_CREAT
            // 创建新文件
            char name[DIRSIZ];
            struct inode *dp = path_to_pinode(path, name);
            if (dp == NULL) {
                return NULL;
            }
            
            ip = inode_alloc(dp->dev, T_FILE);
            if (ip == NULL) {
                inode_put(dp);
                return NULL;
            }
            
            if (dir_add_entry(dp, ip->inum, name) < 0) {
                inode_put(ip);
                inode_put(dp);
                return NULL;
            }
            
            inode_put(dp);
        } else {
            return NULL;
        }
    }
    
    // 分配文件结构
    struct file *f = file_alloc();
    if (f == NULL) {
        inode_put(ip);
        return NULL;
    }
    
    f->type = FD_INODE;
    f->ip = ip;
    f->readable = (omode & 0x1) != 0;  // O_RDONLY
    f->writable = (omode & 0x2) != 0;  // O_WRONLY or O_RDWR
    f->off = 0;
    
    return f;
}

// 读取文件（内核缓冲区版本）
int file_read(struct file* f, void* dst, int n) {
    if (f == NULL || f->readable == 0) {
        return -1;
    }
    
    if (f->type == FD_INODE && f->ip) {
        int r = inode_read_data(f->ip, f->off, dst, n);
        if (r > 0) {
            f->off += r;
        }
        return r;
    }
    
    return -1;
}

// 写入文件（内核缓冲区版本）
int file_write(struct file* f, const void* src, int n) {
    if (f == NULL || f->writable == 0) {
        return -1;
    }
    
    if (f->type == FD_INODE && f->ip) {
        int r = inode_write_data(f->ip, f->off, src, n);
        if (r > 0) {
            f->off += r;
        }
        return r;
    }
    
    return -1;
}

