#ifndef __PROC_H__
#define __PROC_H__

#include "common.h"
#include "mem/pmem.h" // 需要页表定义
#include "mem/kvm.h"  // 需要pgtbl_t定义

// [cite: 57, 63] 上下文结构体，用于进程切换
// 保存被调用者保存寄存器(callee-saved registers)
typedef struct context {
    uint64 ra;
    uint64 sp;

    // callee-saved
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
} context_t;

// [cite: 70-75] trapframe定义，用于用户态和内核态切换的暂存区
typedef struct trapframe {
    /* 0 */ uint64 kernel_satp;   // kernel page table
    /* 8 */ uint64 kernel_sp;     // top of process's kernel stack
    /* 16 */ uint64 kernel_trap;  // usertrap()
    /* 24 */ uint64 epc;          // saved user program counter
    /* 32 */ uint64 kernel_hartid; // saved kernel tp
    /* 40 */ uint64 ra;
    /* 48 */ uint64 sp;
    /* 56 */ uint64 gp;
    /* 64 */ uint64 tp;
    /* 72 */ uint64 t0;
    /* 80 */ uint64 t1;
    /* 88 */ uint64 t2;
    /* 96 */ uint64 s0;
    /* 104 */ uint64 s1;
    /* 112 */ uint64 a0;
    /* 120 */ uint64 a1;
    /* 128 */ uint64 a2;
    /* 136 */ uint64 a3;
    /* 144 */ uint64 a4;
    /* 152 */ uint64 a5;
    /* 160 */ uint64 a6;
    /* 168 */ uint64 a7;
    /* 176 */ uint64 s2;
    /* 184 */ uint64 s3;
    /* 192 */ uint64 s4;
    /* 200 */ uint64 s5;
    /* 208 */ uint64 s6;
    /* 216 */ uint64 s7;
    /* 224 */ uint64 s8;
    /* 232 */ uint64 s9;
    /* 240 */ uint64 s10;
    /* 248 */ uint64 s11;
    /* 256 */ uint64 t3;
    /* 264 */ uint64 t4;
    /* 272 */ uint64 t5;
    /* 280 */ uint64 t6;
} trapframe_t;

// [cite: 36-51] 进程控制块定义
typedef struct proc {
    int pid;
    uint64 ustack_pages; // 用户栈占用的页面数量
    
    pgtbl_t pgtbl;       // 用户态页表 [cite: 44]
    uint64 heap_top;     // 用户堆顶 [cite: 45]
    trapframe_t* tf;     // 切换时的暂存空间 [cite: 46]
    uint64 kstack;       // 内核栈的虚拟地址 [cite: 47]
    context_t ctx;       // 内核态进程上下文 [cite: 49]
} proc_t;

// 进程管理函数
proc_t* myproc(void);
pgtbl_t proc_pgtbl_init(uint64 trapframe_pa);
void proc_make_first(void);
void swtch(context_t *old, context_t *new);

#endif