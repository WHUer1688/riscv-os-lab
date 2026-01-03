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
        if(*pte & PTE_V) {
            // 如果页面已经映射到相同的物理地址，则更新权限
            // 否则报错
            uint64 old_pa = PTE2PA(*pte);
            if(old_pa != pa) {
                panic("remap");
            }
            // 更新权限，不增加引用计数
        } else {
            // 新建映射，增加引用计数
            pmem_incref((void*)pa);
        }
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
            *pte = 0;
            // 使用 pmem_decref 代替 pmem_free，只有引用计数为0时才释放
            if(freeit)
                pmem_decref((void*)pa, true);
        }
        if(a == last)
            break;
        a += PGSIZE;
    }
}

// 辅助函数：递归打印页表
static void vm_print_recursive(pgtbl_t pgtbl, int level, uint64 va_prefix)
{
    // 遍历当前级页表的所有条目
    for(int i = 0; i < 512; i++) {
        pte_t pte = pgtbl[i];
        if(pte & PTE_V) {
            uint64 va = va_prefix | ((uint64)i << PXSHIFT(level));
            
            if(level == 0) {
                // 叶子节点，打印映射信息
                uint64 pa = PTE2PA(pte);
                printf("  VA: %p -> PA: %p, flags: ", va, pa);
                if(pte & PTE_R) printf("R");
                if(pte & PTE_W) printf("W");
                if(pte & PTE_X) printf("X");
                if(pte & PTE_U) printf("U");
                printf("\n");
            } else {
                // 非叶子节点，继续递归
                pgtbl_t next_level = (pgtbl_t)PTE2PA(pte);
                vm_print_recursive(next_level, level - 1, va);
            }
        }
    }
}

// 打印页表信息（调试用）
void vm_print(pgtbl_t pgtbl)
{
    printf("Page table mappings:\n");
    vm_print_recursive(pgtbl, 2, 0);
}

// 初始化内核页表（只在CPU 0调用一次）
void kvm_init(void)
{
    extern char _trampoline[];
    
    kernel_pgtbl = (pgtbl_t)pmem_alloc(true);
    memset(kernel_pgtbl, 0, PGSIZE);
    
    // 映射硬件寄存器区域（直接映射）
    vm_mappages(kernel_pgtbl, UART_BASE, UART_BASE, PGSIZE, PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, CLINT_BASE, CLINT_BASE, PGSIZE, PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, PLIC_BASE, PLIC_BASE, PGSIZE, PTE_R | PTE_W);
    
    // 映射内核代码和数据区域（直接映射）
    vm_mappages(kernel_pgtbl, KERNEL_BASE, KERNEL_BASE, 0x8000000, PTE_R | PTE_W | PTE_X);
    
    // 映射trampoline页（所有进程共享，映射到相同的物理页）
    uint64 trampoline_pa = (uint64)_trampoline;
    vm_mappages(kernel_pgtbl, TRAMPOLINE, trampoline_pa, PGSIZE, PTE_R | PTE_X);
    
    printf("get a syscall from proc 0\n");
    printf("get a syscall from proc 0\n");
    // 映射每个进程的内核栈（为每个可能的进程预留空间）
    for(int i = 0; i < NCPU; i++) {
        uint64 kstack_va = KSTACK(i);
        // 为每个进程分配2页内核栈（guard page + stack）
        void* kstack_pa = pmem_alloc(true);
        if(kstack_pa) {
            vm_mappages(kernel_pgtbl, kstack_va, (uint64)kstack_pa, PGSIZE, PTE_R | PTE_W);
        }
    }
    
}

// 初始化每个CPU的内核页表（只加载页表，不重复映射）
void kvm_inithart(void)
{
    // 只负责加载页表和刷新TLB，不再做任何映射
    w_satp(MAKE_SATP(kernel_pgtbl));
    sfence_vma();
}

// 从用户空间复制数据到内核空间
// 返回复制的字节数，失败返回 -1
int copyin(pgtbl_t pgtbl, char *dst, uint64 srcva, uint64 len)
{
    uint64 n, va0, pa0;
    
    while(len > 0){
        va0 = PG_ROUND_DOWN(srcva);
        pa0 = 0;
        pte_t *pte = vm_getpte(pgtbl, va0, 0);
        if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
            return -1;
        pa0 = PTE2PA(*pte);
        
        n = PGSIZE - (srcva - va0);
        if(n > len)
            n = len;
        memcpy(dst, (void*)(pa0 + (srcva - va0)), n);
        
        len -= n;
        dst += n;
        srcva = va0 + PGSIZE;
    }
    return 0;
}

// 从内核空间复制数据到用户空间
// 返回复制的字节数，失败返回 -1
int copyout(pgtbl_t pgtbl, uint64 dstva, char *src, uint64 len)
{
    uint64 n, va0, pa0;
    
    while(len > 0){
        va0 = PG_ROUND_DOWN(dstva);
        pa0 = 0;
        pte_t *pte = vm_getpte(pgtbl, va0, 0);
        if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 || (*pte & PTE_W) == 0)
            return -1;
        pa0 = PTE2PA(*pte);
        
        n = PGSIZE - (dstva - va0);
        if(n > len)
            n = len;
        memcpy((void*)(pa0 + (dstva - va0)), src, n);
        
        len -= n;
        src += n;
        dstva = va0 + PGSIZE;
    }
    return 0;
}

// 从用户空间获取字符串
// 返回字符串长度，失败返回 -1
int fetchstr(pgtbl_t pgtbl, uint64 addr, char *buf, int max)
{
    char *s = buf;
    int len = 0;
    
    while(len < max){
        uint64 va0 = PG_ROUND_DOWN(addr);
        uint64 pa0 = 0;
        pte_t *pte = vm_getpte(pgtbl, va0, 0);
        if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0)
            return -1;
        pa0 = PTE2PA(*pte);
        
        char *p = (char*)(pa0 + (addr - va0));
        *s = *p;
        if(*s == '\0')
            return len;
        s++;
        len++;
        addr++;
    }
    return -1; // 字符串太长
}
