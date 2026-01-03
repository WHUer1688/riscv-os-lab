#ifndef __FS_H__
#define __FS_H__

#include "common.h"
#include "buf.h"

// Super block块号
#define SB_BLOCK 1

// Super block结构
struct super_block {
    uint32 magic;           // 文件系统魔数
    uint32 size;            // 文件系统大小（块数）
    uint32 nblocks;         // 数据块数
    uint32 ninodes;         // inode数
    uint32 nlog;            // 日志块数（本实验不使用）
    uint32 logstart;       // 日志起始块（本实验不使用）
    uint32 inodestart;     // inode起始块
    uint32 bmapstart;      // 数据bitmap起始块
    uint32 datastart;      // 数据块起始块
};

// 全局super block
extern struct super_block sb;

// 文件系统初始化
void fs_init(void);

#endif

