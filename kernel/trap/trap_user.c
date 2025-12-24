#include "proc/proc.h"
#include "riscv.h"
#include "lib/print.h"
#include "memlayout.h"

extern void user_return(void);

// 函数声明
void trap_user_return(trapframe_t* tf);

// 用户态trap处理函数
void trap_user_handler(trapframe_t* tf)
{
    uint64 scause = r_scause();
    
    // 检查是否是系统调用（ecall）
    // ecall的scause值通常是8（Environment call from U-mode）
    if(scause == 8) {
        // 系统调用
        proc_t *p = myproc();
        if(p) {
            printf("get a syscall from proc %d\n", p->pid);
        }
        
        // 前进PC（跳过ecall指令，通常是4字节）
        // 需要更新trapframe中的epc，因为user_return会使用它
        uint64 sepc = r_sepc();
        p->tf->epc = sepc + 4;
    } else {
        // 其他异常/中断：打印信息便于调试
        printf("user trap: scause=%lx stval=%lx sepc=%lx\n", scause, r_stval(), r_sepc());
        for(;;) { }
    }
    
    // 返回用户态
    trap_user_return(tf);
}

// 从内核返回用户态
void trap_user_return(trapframe_t* tf)
{
    proc_t *p = myproc();
    if(!p || !p->tf) {
        panic("trap_user_return: no proc or tf");
    }
    
    // 设置从S回U的必要状态
    // 清除SPP（表示返回用户态）
    uint64 sstatus = r_sstatus();
    sstatus &= ~SSTATUS_SPP;
    // 设置SPIE（允许中断）
    sstatus |= SSTATUS_SPIE;
    w_sstatus(sstatus);
    
    // 设置sepc为用户epc
    w_sepc(p->tf->epc);
    
    // 设置sscratch为trapframe地址（用于下次trap时保存寄存器）
    w_sscratch((uint64)p->tf);
    
    // 设置stvec为用户trap向量
    extern void user_vector(void);
    w_stvec((uint64)user_vector);
    
    // 切换到用户页表（trampoline在用户页表中也映射了）
    w_satp(MAKE_SATP(p->pgtbl));
    sfence_vma();
    
    // 跳转到trampoline的user_return，最终sret回到用户态
    // user_return需要trapframe地址在a0中
    // user_return在trampoline中，在用户页表中映射到TRAMPOLINE地址
    // 我们需要计算user_return在trampoline中的偏移，然后加上TRAMPOLINE地址
    extern void user_return(void);
    extern void user_vector(void);
    uint64 user_return_offset = (uint64)user_return - (uint64)user_vector;
    uint64 user_return_va = TRAMPOLINE + user_return_offset;
    
    // trapframe在用户页表中映射到TRAMPOLINE - PGSIZE
    uint64 trapframe_user_va = TRAMPOLINE - PGSIZE;
    
    // 使用内联汇编将trapframe的用户虚拟地址放到a0，然后跳转到user_return的用户虚拟地址
    asm volatile(
        "mv a0, %0\n\t"
        "jalr zero, %1, 0"
        :
        : "r" (trapframe_user_va), "r" (user_return_va)
        : "a0"
    );
}

