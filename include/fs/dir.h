#ifndef __DIR_H__
#define __DIR_H__

#include "common.h"
#include "inode.h"

// 目录项结构
#define DIRSIZ 14
struct dirent {
    uint16 inum;
    char name[DIRSIZ];
};

// 目录操作
int dir_add_entry(struct inode* dp, uint16 inum, const char* name);
struct inode* dir_lookup(struct inode* dp, const char* name, uint16* poff);

// 路径解析
struct inode* path_to_pinode(const char* path, char* name);
struct inode* path_to_inode(const char* path);

#endif

