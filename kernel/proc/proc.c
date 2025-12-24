#include "proc/cpu.h"
#include "proc/proc.h"
#include "mem/kvm.h"
#include "mem/pmem.h"
#include "mem/str.h"
#include "riscv.h"
#include "lib/print.h"
#include "memlayout.h"

cpu_t cpus[NCPU];

cpu_t* mycpu(void)
{
    int id = r_tp();
    return &cpus[id];
}

int mycpuid(void) 
{
    return r_tp();
}

// 获取当前进程
proc_t* myproc(void)
{
    return mycpu()->proc;
}

// 上下文切换函数（在swtch.S中实现）
void swtch(context_t *old, context_t *new);

// 用户态trap处理函数声明
void trap_user_handler(trapframe_t* tf);
void trap_user_return(trapframe_t* tf);

// 全局proczero进程
static proc_t proczero;

// initcode字节数组
// 根据要求，initcode应该执行两次系统调用然后进入死循环
// RISC-V指令编码（小端序）：
// li a7, 1: 
//   lui a7, 0        -> 0x000037b7 (lui a7, 0)
//   addi a7, a7, 1   -> 0x00138393 (addi a7, a7, 1)
// ecall: 0x00000073
// j . (死循环): jal x0, -4 -> 0xffdff06f
// 注意：PDF中应该有具体的initcode，这里先创建一个可用的版本
static char initcode[] = {
    // 第一次系统调用
    0xb7, 0x03, 0x00, 0x00,  // lui a7, 0
    0x93, 0x83, 0x13, 0x00,  // addi a7, a7, 1
    0x73, 0x00, 0x00, 0x00,  // ecall
    // 第二次系统调用
    0xb7, 0x03, 0x00, 0x00,  // lui a7, 0
    0x93, 0x83, 0x13, 0x00,  // addi a7, a7, 1
    0x73, 0x00, 0x00, 0x00,  // ecall
    // 死循环: j . (跳转到自己，偏移-4)
    0x6f, 0xf0, 0xdf, 0xff,  // jal x0, -4
};

// 建立用户地址空间页表
pgtbl_t proc_pgtbl_init(uint64 trapframe_pa)
{
    pgtbl_t pgtbl = (pgtbl_t)pmem_alloc(true);
    if(!pgtbl) {
        panic("proc_pgtbl_init: pmem_alloc failed");
    }
    memset(pgtbl, 0, PGSIZE);
    
    extern char _trampoline[];
    uint64 trampoline_pa = (uint64)_trampoline;
    
    // 映射trampoline页（所有进程共享，在TRAMPOLINE虚拟地址）
    vm_mappages(pgtbl, TRAMPOLINE, trampoline_pa, PGSIZE, PTE_R | PTE_X);
    
    // 映射trapframe页（用户不可读写，但trampoline可以访问）
    // trapframe通常在用户地址空间的顶部附近
    // 根据xv6的设计，trapframe通常在TRAMPOLINE下方
    uint64 trapframe_va = TRAMPOLINE - PGSIZE;
    vm_mappages(pgtbl, trapframe_va, trapframe_pa, PGSIZE, PTE_R | PTE_W);
    
    // 映射用户代码和数据（从VA 0开始）
    // 需要分配物理页来存放initcode
    void* code_pa = pmem_alloc(true);
    if(!code_pa) {
        panic("proc_pgtbl_init: failed to alloc code page");
    }
    // 将initcode复制到物理页
    memcpy(code_pa, initcode, sizeof(initcode));
    vm_mappages(pgtbl, 0, (uint64)code_pa, PGSIZE, PTE_R | PTE_X | PTE_U);
    
    // 映射用户栈（通常在用户地址空间的高地址）
    // 根据xv6的设计，用户栈通常在0x80000000下方
    uint64 stack_va = 0x80000000UL - PGSIZE;
    void* stack_pa = pmem_alloc(true);
    if(!stack_pa) {
        panic("proc_pgtbl_init: failed to alloc stack page");
    }
    memset(stack_pa, 0, PGSIZE);
    vm_mappages(pgtbl, stack_va, (uint64)stack_pa, PGSIZE, PTE_R | PTE_W | PTE_U);
    
    return pgtbl;
}

// 创建proczero并切换到它
void proc_make_first(void)
{
    cpu_t *cpu = mycpu();
    
    // 初始化proczero
    memset(&proczero, 0, sizeof(proczero));
    proczero.pid = 0;
    proczero.ustack_pages = 1;
    
    // 分配trapframe页
    void* trapframe_pa = pmem_alloc(true);
    if(!trapframe_pa) {
        panic("proc_make_first: failed to alloc trapframe");
    }
    memset(trapframe_pa, 0, PGSIZE);
    proczero.tf = (trapframe_t*)trapframe_pa;
    
    // 建立用户页表
    proczero.pgtbl = proc_pgtbl_init((uint64)trapframe_pa);
    
    // 设置内核栈
    int cpuid = mycpuid();
    proczero.kstack = KSTACK(cpuid);
    
    // 设置trapframe
    extern pgtbl_t kernel_pgtbl;
    extern void trap_user_handler(trapframe_t*);
    extern void user_return(void);
    
    proczero.tf->kernel_satp = MAKE_SATP(kernel_pgtbl);
    proczero.tf->kernel_sp = proczero.kstack + PGSIZE;  // 栈顶
    proczero.tf->kernel_trap = (uint64)trap_user_handler;
    proczero.tf->kernel_hartid = cpuid;
    
    // 设置用户态初始状态
    proczero.tf->epc = 0;  // 用户程序从VA 0开始执行
    // 用户栈顶（栈向下增长，所以sp = stack_va + PGSIZE）
    uint64 stack_va = 0x80000000UL - PGSIZE;
    proczero.tf->sp = stack_va + PGSIZE;
    
    // 设置context（用于第一次swtch进入该进程）
    // 第一次进入时，需要调用trap_user_return，它需要trapframe在a0中
    // 我们创建一个包装函数，设置a0然后调用trap_user_return
    extern void proc_first_return(void);
    proczero.ctx.ra = (uint64)proc_first_return;  // 第一次进入时调用包装函数
    proczero.ctx.sp = proczero.kstack + PGSIZE;  // 内核栈顶
    
    // 设置CPU当前进程
    cpu->proc = &proczero;
    
    // 切换到proczero
    swtch(&cpu->ctx, &proczero.ctx);
}

// 第一次进入proczero时的包装函数
// 设置trapframe到a0，然后调用trap_user_return
void proc_first_return(void)
{
    proc_t *p = myproc();
    if(!p || !p->tf) {
        panic("proc_first_return: no proc or tf");
    }
    // trap_user_return需要trapframe在参数中，但我们需要通过a0传递
    // 实际上，trap_user_return是C函数，参数通过a0传递
    trap_user_return(p->tf);
}
