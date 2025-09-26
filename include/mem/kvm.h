#ifndef __KVM_H__
#define __KVM_H__

#include "common.h"
#include "memlayout.h"

// 页表项
typedef uint64 pte_t;

// 页表
typedef uint64* pgtbl_t;

// 页表项标志位
#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1) // readable
#define PTE_W (1L << 2) // writable
#define PTE_X (1L << 3) // executable
#define PTE_U (1L << 4) // user accessible

// 地址转换宏
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
#define PTE2PA(pte) (((pte) >> 10) << 12)
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 页表索引宏
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (12+(9*(level)))
#define PX(level, va)   ((((uint64) (va)) >> PXSHIFT(level)) & PXMASK)

// 最大虚拟地址
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))

// 虚拟内存管理函数
pte_t *vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc);
void vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm);
void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit);
void vm_print(pgtbl_t pgtbl);

// 内核页表管理函数
void kvm_init(void);
void kvm_inithart(void);

// 全局内核页表
extern pgtbl_t kernel_pgtbl;

#endif
