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
    
    // 先同步一下 sepc 到 tf
    tf->epc = r_sepc();
    
    if (scause == 8) { // 8 = ecall from U-mode
        printf("get a syscall from proc %d\n", myproc()->pid);
        
        // 必须跳过 ecall 指令，否则会无限陷入->无限打印
        tf->epc += 4;
        
        trap_user_return(tf); // 回到用户态执行下一条（第二次 ecall）
        return;
    }
    
    for(;;) {}
}

// 从内核返回用户态
void trap_user_return(trapframe_t* tf)
{
    proc_t *p = myproc();
    if(!p || !p->pgtbl) {
        panic("trap_user_return: no proc or pgtbl");
    }
    
    // 设置从S回U的必要状态
    // 清除SPP（表示返回用户态）
    uint64 sstatus = r_sstatus();
    sstatus &= ~SSTATUS_SPP;
    // 设置SPIE（允许中断）
    sstatus |= SSTATUS_SPIE;
    w_sstatus(sstatus);
    
    // 设置sepc为用户epc（使用传入的tf）
    w_sepc(tf->epc);
    
    // 设置sscratch为trapframe的用户虚拟地址（用于下次trap时保存寄存器）
    uint64 trapframe_user_va = TRAMPOLINE - PGSIZE;
    w_sscratch(trapframe_user_va);
    
    // 设置stvec为用户trap向量（必须使用用户页表中的TRAMPOLINE地址）
    // user_vector 在 trampoline 页首，所以直接使用 TRAMPOLINE
    w_stvec(TRAMPOLINE);
    
    // 切换到用户页表（trampoline在用户页表中也映射了）
    w_satp(MAKE_SATP(p->pgtbl));
    sfence_vma();
    
    // 跳转到trampoline的user_return，最终sret回到用户态
    // user_return需要trapframe地址在a0中
    extern void user_return(void);
    extern void user_vector(void);
    uint64 user_return_offset = (uint64)user_return - (uint64)user_vector;
    uint64 user_return_va = TRAMPOLINE + user_return_offset;
    
    // 使用内联汇编将trapframe的用户虚拟地址放到a0，然后跳转到user_return的用户虚拟地址
    asm volatile(
        "mv a0, %0\n\t"
        "jalr zero, %1, 0"
        :
        : "r" (trapframe_user_va), "r" (user_return_va)
        : "a0"
    );
}

