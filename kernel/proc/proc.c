#include "proc/cpu.h"
#include "proc/proc.h"
#include "mem/kvm.h"
#include "mem/pmem.h"
#include "mem/str.h"
#include "riscv.h"
#include "lib/print.h"
#include "memlayout.h"
#include "lib/lock.h"
#include "fs/buf.h"
#include "fs/fs.h"

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

// 全局进程数组和锁
static proc_t procs[NPROC];
static proc_t* proczero;
static int global_pid = 1;  // 从1开始分配，0给proczero
static spinlock_t lk_pid;

// 默认时间片
#define DEFAULT_SLICE 10

// initcode字节数组
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
    
    // 映射trapframe页
    uint64 trapframe_va = TRAMPOLINE - PGSIZE;
    vm_mappages(pgtbl, trapframe_va, trapframe_pa, PGSIZE, PTE_R | PTE_W);
    
    // 映射用户代码和数据（从VA 0开始）
    void* code_pa = pmem_alloc(true);
    if(!code_pa) {
        panic("proc_pgtbl_init: failed to alloc code page");
    }
    memcpy(code_pa, initcode, sizeof(initcode));
    vm_mappages(pgtbl, 0, (uint64)code_pa, PGSIZE, PTE_R | PTE_X | PTE_U);
    
    // 映射用户栈
    uint64 stack_va = 0x80000000UL - PGSIZE;
    void* stack_pa = pmem_alloc(true);
    if(!stack_pa) {
        panic("proc_pgtbl_init: failed to alloc stack page");
    }
    memset(stack_pa, 0, PGSIZE);
    vm_mappages(pgtbl, stack_va, (uint64)stack_pa, PGSIZE, PTE_R | PTE_W | PTE_U);
    
    return pgtbl;
}

// 释放用户页表和用户内存
static void proc_freepgtbl(pgtbl_t pgtbl)
{
    // 释放用户地址空间的所有映射（除了trampoline，它是共享的）
    // 释放代码段
    vm_unmappages(pgtbl, 0, PGSIZE, true);
    // 释放栈
    uint64 stack_va = 0x80000000UL - PGSIZE;
    vm_unmappages(pgtbl, stack_va, PGSIZE, true);
    // 释放trapframe
    uint64 trapframe_va = TRAMPOLINE - PGSIZE;
    vm_unmappages(pgtbl, trapframe_va, PGSIZE, true);
    // 释放页表本身
    pmem_free(pgtbl, true);
}

// 初始化进程系统
void proc_init(void)
{
    // 初始化pid锁
    spinlock_init(&lk_pid, "pid");
    
    // 初始化每个进程
    for(int i = 0; i < NPROC; i++) {
        proc_t *p = &procs[i];
        // 初始化进程锁
        spinlock_init(&p->lk, "proc");
        
        // 计算内核栈虚拟地址（每个进程使用不同的kstack）
        // 注意：这里需要确保kstack映射存在（在kvm_init中已映射）
        p->kstack = KSTACK(i);
        
        // 初始化为UNUSED状态
        p->state = UNUSED;
    }
    
    // 创建proczero（pid=0）
    proczero = proc_alloc();
    if(!proczero) {
        panic("proc_init: failed to alloc proczero");
    }
    proczero->pid = 0;
    proczero->parent = NULL;
    proczero->state = RUNNABLE;
}

// 分配一个新进程
proc_t* proc_alloc(void)
{
    proc_t *p;
    
    // 从数组找UNUSED进程
    for(p = procs; p < &procs[NPROC]; p++) {
        spinlock_acquire(&p->lk);
        if(p->state == UNUSED) {
            goto found;
        }
        spinlock_release(&p->lk);
    }
    return NULL;  // 没有可用进程
    
found:
    // 分配PID
    spinlock_acquire(&lk_pid);
    p->pid = global_pid++;
    spinlock_release(&lk_pid);
    
    // 初始化进程字段
    p->state = USED;
    p->parent = NULL;
    p->exit_state = 0;
    p->sleep_space = NULL;
    p->ustack_pages = 1;
    p->heap_top = 0;
    p->time_slice = DEFAULT_SLICE;
    // 初始化文件描述符数组
    for (int i = 0; i < 16; i++) {
        p->ofile[i] = NULL;
    }
    
    // 分配trapframe
    void* trapframe_pa = pmem_alloc(true);
    if(!trapframe_pa) {
        p->state = UNUSED;
        spinlock_release(&p->lk);
        return NULL;
    }
    memset(trapframe_pa, 0, PGSIZE);
    p->tf = (trapframe_t*)trapframe_pa;
    
    // 建立用户页表
    p->pgtbl = proc_pgtbl_init((uint64)trapframe_pa);
    if(!p->pgtbl) {
        pmem_free(trapframe_pa, true);
        p->state = UNUSED;
        spinlock_release(&p->lk);
        return NULL;
    }
    
    // 初始化context（第一次调度会返回到fork_return）
    extern void fork_return(void);
    p->ctx.ra = (uint64)fork_return;
    p->ctx.sp = p->kstack + PGSIZE;
    // 其他callee-saved寄存器初始化为0
    p->ctx.s0 = 0;
    p->ctx.s1 = 0;
    p->ctx.s2 = 0;
    p->ctx.s3 = 0;
    p->ctx.s4 = 0;
    p->ctx.s5 = 0;
    p->ctx.s6 = 0;
    p->ctx.s7 = 0;
    p->ctx.s8 = 0;
    p->ctx.s9 = 0;
    p->ctx.s10 = 0;
    p->ctx.s11 = 0;
    
    spinlock_release(&p->lk);
    return p;
}

// 释放进程资源
void proc_free(proc_t *p)
{
    if(!p) return;
    
    spinlock_acquire(&p->lk);
    
    // 释放用户页表和用户内存
    if(p->pgtbl) {
        proc_freepgtbl(p->pgtbl);
        p->pgtbl = NULL;
    }
    
    // 释放trapframe
    if(p->tf) {
        pmem_free(p->tf, true);
        p->tf = NULL;
    }
    
    // 清空字段
    p->pid = 0;
    p->parent = NULL;
    p->exit_state = 0;
    p->sleep_space = NULL;
    p->state = UNUSED;
    
    spinlock_release(&p->lk);
}

// fork_return：子进程第一次被调度时的入口
void fork_return(void)
{
    proc_t *p = myproc();
    if(!p || !p->tf) {
        panic("fork_return: no proc or tf");
    }
    
    // 第一次返回用户态前初始化文件系统（只在proczero时执行一次）
    static int fs_initialized = 0;
    if (!fs_initialized && p->pid == 0) {
        buf_init();
        fs_init();
        fs_initialized = 1;
    }
    
    // 返回到用户态
    trap_user_return(p->tf);
}

// 创建proczero并切换到它
void proc_make_first(void)
{
    cpu_t *cpu = mycpu();
    
    // 使用proc_alloc创建proczero（已在proc_init中创建）
    proc_t *p = proczero;
    if(!p) {
        panic("proc_make_first: no proczero");
    }
    
    // 设置内核栈
    int cpuid = mycpuid();
    p->kstack = KSTACK(cpuid);
    
    // 设置trapframe
    extern pgtbl_t kernel_pgtbl;
    
    p->tf->kernel_satp = MAKE_SATP(kernel_pgtbl);
    p->tf->kernel_sp = p->kstack + PGSIZE;
    p->tf->kernel_trap = (uint64)trap_user_handler;
    p->tf->kernel_hartid = cpuid;
    
    // 设置用户态初始状态
    p->tf->epc = 0;
    uint64 stack_va = 0x80000000UL - PGSIZE;
    p->tf->sp = stack_va + PGSIZE;
    
    // 设置context（第一次进入时调用fork_return）
    p->ctx.ra = (uint64)fork_return;
    p->ctx.sp = p->kstack + PGSIZE;
    
    // 设置CPU当前进程
    cpu->proc = p;
    spinlock_acquire(&p->lk);
    p->state = RUNNABLE;
    p->time_slice = DEFAULT_SLICE;
    spinlock_release(&p->lk);
    
    // 直接调用fork_return进入用户态（按讲义要求，不再swtch）
    fork_return();
}

// 复制用户内存（页表和物理页）
static int uvmcopy(pgtbl_t old, pgtbl_t new, uint64 sz)
{
    pte_t *pte;
    uint64 pa, i;
    uint64 flags;
    char *mem;
    
    for(i = 0; i < sz; i += PGSIZE){
        if((pte = vm_getpte(old, i, 0)) == NULL)
            panic("uvmcopy: pte should exist");
        if((*pte & PTE_V) == 0)
            panic("uvmcopy: page not present");
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        if((mem = (char*)pmem_alloc(true)) == NULL)
            goto err;
        memcpy(mem, (char*)pa, PGSIZE);
        vm_mappages(new, i, (uint64)mem, PGSIZE, flags);
    }
    return 0;
    
err:
    // 释放已分配的页面
    if(i > 0) {
        vm_unmappages(new, 0, i, true);
    }
    return -1;
}

// fork：创建子进程
int proc_fork(void)
{
    proc_t *cur = myproc();
    proc_t *np;
    
    // 1. 分配新进程
    if((np = proc_alloc()) == NULL) {
        return -1;
    }
    
    // 2. 复制用户内存
    if(uvmcopy(cur->pgtbl, np->pgtbl, PGSIZE) < 0) {  // 只复制代码页
        proc_free(np);
        return -1;
    }
    np->ustack_pages = cur->ustack_pages;
    np->heap_top = cur->heap_top;
    
    // 3. 复制trapframe
    *np->tf = *cur->tf;
    // 子进程返回值置0
    np->tf->a0 = 0;
    
    // 4. 设置父进程
    spinlock_acquire(&np->lk);
    np->parent = cur;
    spinlock_release(&np->lk);
    
    // 5. 复制打开文件/工作目录（暂时跳过，后续实现）
    
    // 6. 设置状态为RUNNABLE
    spinlock_acquire(&np->lk);
    np->state = RUNNABLE;
    np->time_slice = DEFAULT_SLICE;
    spinlock_release(&np->lk);
    
    return np->pid;
}

// reparent：将子进程的父进程改为proczero
void proc_reparent(proc_t *p)
{
    for(proc_t *pp = procs; pp < &procs[NPROC]; pp++) {
        if(pp == p) continue;
        spinlock_acquire(&pp->lk);
        if(pp->parent == p) {
            pp->parent = proczero;
        }
        spinlock_release(&pp->lk);
    }
}

// exit：退出当前进程
void proc_exit(int status)
{
    proc_t *cur = myproc();
    
    // 处理"父死子活"的reparent问题
    proc_reparent(cur);
    
    // 设置退出状态
    spinlock_acquire(&cur->lk);
    cur->exit_state = status;
    cur->state = ZOMBIE;
    proc_t *parent = cur->parent;
    spinlock_release(&cur->lk);
    
    // 唤醒父进程（如果存在）
    if(parent) {
        proc_wakeup_one(parent);
    }
    
    // 让出CPU（不再返回用户态）
    proc_sched();
    panic("proc_exit: zombie returned");
}

// wait：等待子进程退出
int proc_wait(uint64 addr)
{
    proc_t *cur = myproc();
    proc_t *p;
    int havekids;
    int pid;
    
    for(;;) {
        // 扫描所有进程，找子进程
        havekids = 0;
        for(p = procs; p < &procs[NPROC]; p++) {
            spinlock_acquire(&p->lk);
            if(p->parent == cur) {
                havekids = 1;
                if(p->state == ZOMBIE) {
                    // 找到僵尸子进程
                    pid = p->pid;
                    // 复制退出状态到用户空间
                    if(addr != 0 && copyout(cur->pgtbl, addr, (char*)&p->exit_state, sizeof(int)) < 0) {
                        spinlock_release(&p->lk);
                        return -1;
                    }
                    // 回收子进程
                    proc_free(p);
                    spinlock_release(&p->lk);
                    return pid;
                }
            }
            spinlock_release(&p->lk);
        }
        
        // 有子进程但都没退出，sleep等待
        if(havekids) {
            // 注意：proc_sleep需要持有cur->lk，但我们已经释放了
            // 需要重新获取
            spinlock_acquire(&cur->lk);
            proc_sleep(cur, &cur->lk);
            // proc_sleep返回后会重新持有cur->lk
            spinlock_release(&cur->lk);
        } else {
            // 没有子进程
            return -1;
        }
    }
}

// sleep：让当前进程进入睡眠
void proc_sleep(void *chan, spinlock_t *lk)
{
    proc_t *p = myproc();
    
    // 获取进程锁
    spinlock_acquire(&p->lk);
    
    // 释放外部锁
    if(lk != &p->lk) {
        spinlock_release(lk);
    }
    
    // 设置sleep状态
    p->sleep_space = chan;
    p->state = SLEEPING;
    
    // 让出CPU（会释放进程锁）
    proc_sched();
    
    // 被唤醒后：清sleep_space，重新获取外部锁
    // 注意：此时进程锁已经被proc_sched释放了
    spinlock_acquire(&p->lk);
    p->sleep_space = NULL;
    spinlock_release(&p->lk);
    
    if(lk != &p->lk) {
        spinlock_acquire(lk);
    }
}

// wakeup_one：唤醒指定进程
void proc_wakeup_one(proc_t *p)
{
    if(!p) return;
    spinlock_acquire(&p->lk);
    if(p->state == SLEEPING) {
        p->state = RUNNABLE;
        p->time_slice = DEFAULT_SLICE;
    }
    spinlock_release(&p->lk);
}

// wakeup：唤醒所有在chan上sleep的进程
void proc_wakeup(void *chan)
{
    proc_t *p;
    
    for(p = procs; p < &procs[NPROC]; p++) {
        spinlock_acquire(&p->lk);
        if(p->state == SLEEPING && p->sleep_space == chan) {
            p->state = RUNNABLE;
            p->time_slice = DEFAULT_SLICE;
        }
        spinlock_release(&p->lk);
    }
}

// yield：让出CPU
void proc_yield(void)
{
    proc_t *p = myproc();
    spinlock_acquire(&p->lk);
    p->state = RUNNABLE;
    p->time_slice = DEFAULT_SLICE;
    spinlock_release(&p->lk);
    proc_sched();
}

// sched：调度到另一个进程
void proc_sched(void)
{
    proc_t *p = myproc();
    cpu_t *cpu = mycpu();
    
    // 前置检查
    if(intr_get()) {
        panic("proc_sched: interrupts enabled");
    }
    if(cpu->proc != p) {
        panic("proc_sched: wrong proc");
    }
    if(p->state == RUNNING) {
        panic("proc_sched: running");
    }
    
    // 保存中断状态（当前应该是关闭的）
    int intena = intr_get();
    
    // 释放进程锁（在持有锁的情况下不能调度）
    spinlock_release(&p->lk);
    
    // 切换到调度器上下文
    swtch(&p->ctx, &cpu->ctx);
    
    // 被切回后恢复中断状态
    if(intena) {
        intr_on();
    }
}

// scheduler：调度器主循环
void proc_scheduler(void)
{
    cpu_t *cpu = mycpu();
    cpu->proc = NULL;
    
    for(;;) {
        // 遍历所有进程，找RUNNABLE的
        proc_t *p = NULL;
        for(int i = 0; i < NPROC; i++) {
            proc_t *pp = &procs[i];
            spinlock_acquire(&pp->lk);
            if(pp->state == RUNNABLE) {
                p = pp;
                break;
            }
            spinlock_release(&pp->lk);
        }
        
        if(p) {
            // 找到可运行进程
            p->state = RUNNING;
            cpu->proc = p;
            spinlock_release(&p->lk);
            
            // 切换到该进程
            swtch(&cpu->ctx, &p->ctx);
            
            // 被切回后：重新获取进程锁
            spinlock_acquire(&p->lk);
            cpu->proc = NULL;
            // 锁会在proc_sched中释放
        } else {
            // 没有可运行进程，继续循环
            // 可以在这里添加idle逻辑
        }
    }
}
