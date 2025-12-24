#include "mem/pmem.h"
#include "mem/str.h"
#include "lib/print.h"

// 锁函数声明
void spinlock_init(spinlock_t *lk, char *name);
void spinlock_acquire(spinlock_t *lk);
void spinlock_release(spinlock_t *lk);


// 内核和用户可分配的物理页分开
static alloc_region_t kern_region, user_region;

// 引用计数数组 - 为整个可分配区域的页面维护引用计数
#define MAX_PAGES ((ALLOC_END - ALLOC_BEGIN) / PGSIZE)
static uint32 refcount[MAX_PAGES];
static spinlock_t refcount_lk;

// 将物理地址转换为引用计数数组索引
static inline int pa_to_index(uint64 pa)
{
    if(pa < ALLOC_BEGIN || pa >= ALLOC_END)
        return -1;
    return (pa - ALLOC_BEGIN) / PGSIZE;
}

// 初始化物理内存管理
void pmem_init(void)
{
    // 初始化内核区域
    kern_region.begin = ALLOC_BEGIN;
    kern_region.end = ALLOC_BEGIN + KERNEL_PAGES * PGSIZE;
    kern_region.allocable = KERNEL_PAGES;
    kern_region.list_head.next = NULL;
    spinlock_init(&kern_region.lk, "kern_region");
    
    // 初始化用户区域
    user_region.begin = ALLOC_BEGIN + KERNEL_PAGES * PGSIZE;
    user_region.end = ALLOC_END;
    user_region.allocable = (ALLOC_END - (ALLOC_BEGIN + KERNEL_PAGES * PGSIZE)) / PGSIZE;
    user_region.list_head.next = NULL;
    spinlock_init(&user_region.lk, "user_region");
    
    // 初始化引用计数
    spinlock_init(&refcount_lk, "refcount");
    for(int i = 0; i < MAX_PAGES; i++) {
        refcount[i] = 0;
    }
    
    // 注意：在构建空闲页链表时，我们不直接访问这些物理地址
    // 因为此时还没有启用页表，直接访问可能不安全
    // 我们只是初始化数据结构，实际的页面分配在启用页表后进行
    
    // 暂时不构建链表，避免访问未映射的物理地址
    // 在第一次分配时会动态构建链表
}

// 分配物理页
void* pmem_alloc(bool in_kernel)
{
    alloc_region_t *region = in_kernel ? &kern_region : &user_region;
    
    spinlock_acquire(&region->lk);
    
    // 如果链表为空，动态构建链表
    if(region->list_head.next == NULL) {
        // 构建空闲页链表 - 只有在页表启用后才安全
        for(uint64 pa = region->begin; pa < region->end; pa += PGSIZE) {
            page_node_t *page = (page_node_t*)pa;
            page->next = region->list_head.next;
            region->list_head.next = page;
        }
    }
    
    if(region->list_head.next == NULL) {
        spinlock_release(&region->lk);
        return NULL; // 没有可用页面
    }
    
    page_node_t *page = region->list_head.next;
    region->list_head.next = page->next;
    region->allocable--;
    
    spinlock_release(&region->lk);
    
    // 清零页面
    memset((void*)page, 0, PGSIZE);
    
    // 设置引用计数为1
    int idx = pa_to_index((uint64)page);
    if(idx >= 0) {
        spinlock_acquire(&refcount_lk);
        refcount[idx] = 1;
        spinlock_release(&refcount_lk);
    }
    
    return (void*)page;
}

// 释放物理页
void pmem_free(void* pa, bool in_kernel)
{
    if(pa == NULL) return;
    
    alloc_region_t *region = in_kernel ? &kern_region : &user_region;
    
    // 检查地址是否在有效范围内
    uint64 addr = (uint64)pa;
    if(addr < region->begin || addr >= region->end) {
        return; // 无效地址
    }
    
    spinlock_acquire(&region->lk);
    
    page_node_t *page = (page_node_t*)pa;
    page->next = region->list_head.next;
    region->list_head.next = page;
    region->allocable++;
    
    spinlock_release(&region->lk);
}

// 增加物理页引用计数
void pmem_incref(void* pa)
{
    if(pa == NULL) return;
    
    int idx = pa_to_index((uint64)pa);
    if(idx < 0) return;
    
    spinlock_acquire(&refcount_lk);
    refcount[idx]++;
    spinlock_release(&refcount_lk);
}

// 减少物理页引用计数，当计数为0时释放
void pmem_decref(void* pa, bool in_kernel)
{
    if(pa == NULL) return;
    
    int idx = pa_to_index((uint64)pa);
    if(idx < 0) return;
    
    spinlock_acquire(&refcount_lk);
    if(refcount[idx] > 0) {
        refcount[idx]--;
    }
    uint32 count = refcount[idx];
    spinlock_release(&refcount_lk);
    
    // 只有当引用计数为0时才真正释放物理页
    if(count == 0) {
        pmem_free(pa, in_kernel);
    }
}

// 获取物理页引用计数
uint32 pmem_getref(void* pa)
{
    if(pa == NULL) return 0;
    
    int idx = pa_to_index((uint64)pa);
    if(idx < 0) return 0;
    
    spinlock_acquire(&refcount_lk);
    uint32 count = refcount[idx];
    spinlock_release(&refcount_lk);
    
    return count;
}
