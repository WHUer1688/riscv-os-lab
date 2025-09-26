#include "mem/kvm.h"
#include "mem/pmem.h"
#include "riscv.h"
#include "mem/str.h"
#include "lib/print.h"


// 全局内核页表
pgtbl_t kernel_pgtbl;

// 获取页表项
pte_t *vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc)
{
    if(va >= MAXVA) return NULL;
    
    for(int level = 2; level > 0; level--) {
        pte_t *pte = &pgtbl[PX(level, va)];
        if(*pte & PTE_V) {
            pgtbl = (pgtbl_t)PTE2PA(*pte);
        } else {
            if(!alloc || (pgtbl = (pgtbl_t)pmem_alloc(true)) == NULL)
                return NULL;
            memset(pgtbl, 0, PGSIZE);
            *pte = PA2PTE(pgtbl) | PTE_V;
        }
    }
    return &pgtbl[PX(0, va)];
}

// 映射页面
void vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm)
{
    uint64 a, last;
    pte_t *pte;
    
    a = PG_ROUND_DOWN(va);
    last = PG_ROUND_DOWN(va + len - 1);
    for(;;){
        if((pte = vm_getpte(pgtbl, a, true)) == NULL)
            return;
        if(*pte & PTE_V)
            panic("remap");
        *pte = PA2PTE(pa) | perm | PTE_V;
        if(a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
}

// 取消映射页面
void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit)
{
    uint64 a, last;
    pte_t *pte;
    
    a = PG_ROUND_DOWN(va);
    last = PG_ROUND_DOWN(va + len - 1);
    for(;;){
        if((pte = vm_getpte(pgtbl, a, false)) == NULL)
            return;
        if(*pte & PTE_V){
            uint64 pa = PTE2PA(*pte);
            if(freeit)
                pmem_free((void*)pa, true);
            *pte = 0;
        }
        if(a == last)
            break;
        a += PGSIZE;
    }
}

// 打印页表信息（调试用）
void vm_print(pgtbl_t pgtbl)
{
    // 简单的页表打印实现
    // 这里可以根据需要实现更详细的打印功能
}

// 初始化内核页表（只在CPU 0调用一次）
void kvm_init(void)
{
    kernel_pgtbl = (pgtbl_t)pmem_alloc(true);
    memset(kernel_pgtbl, 0, PGSIZE);
    
    // 映射硬件寄存器区域（直接映射）
    vm_mappages(kernel_pgtbl, UART_BASE, UART_BASE, PGSIZE, PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, CLINT_BASE, CLINT_BASE, PGSIZE, PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, PLIC_BASE, PLIC_BASE, PGSIZE, PTE_R | PTE_W);
    
    // 映射内核代码和数据区域（直接映射）
    vm_mappages(kernel_pgtbl, KERNEL_BASE, KERNEL_BASE, 0x8000000, PTE_R | PTE_W | PTE_X);
}

// 初始化每个CPU的内核页表（只加载页表，不重复映射）
void kvm_inithart(void)
{
    // 只负责加载页表和刷新TLB，不再做任何映射
    w_satp(MAKE_SATP(kernel_pgtbl));
    sfence_vma();
}
