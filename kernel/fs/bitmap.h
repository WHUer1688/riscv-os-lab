#ifndef __BITMAP_H__
#define __BITMAP_H__

#include "common.h"

// 数据块分配
uint32 balloc(void);

// 数据块释放
void bfree(uint32 blockno);

// inode分配
uint32 ialloc(void);

// inode释放
void ifree(uint32 inum);

#endif

