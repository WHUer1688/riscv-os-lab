#ifndef __PMEM_H__
#define __PMEM_H__

#include "common.h"
#include "lib/lock.h"
#include "memlayout.h"

// 物理页节点
typedef struct page_node {
    struct page_node* next;
} page_node_t;

// 可分配区域
typedef struct alloc_region {
    uint64 begin;           // 起始物理地址
    uint64 end;             // 终止物理地址
    spinlock_t lk;          // 自旋锁
    uint32 allocable;       // 可分配页面数
    page_node_t list_head;  // 可分配链的链头节点
} alloc_region_t;

// 物理内存管理函数
void pmem_init(void);
void* pmem_alloc(bool in_kernel);
void pmem_free(void* pa, bool in_kernel);

// 引用计数管理函数
void pmem_incref(void* pa);
void pmem_decref(void* pa, bool in_kernel);
uint32 pmem_getref(void* pa);

#endif
