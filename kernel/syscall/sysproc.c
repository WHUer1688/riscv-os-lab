#include "syscall.h"
#include "proc/proc.h"
#include "lib/print.h"
#include "mem/str.h"
#include "mem/kvm.h"

// 声明参数获取函数（在 syscall.c 中定义）
int argint(int n, int *ip);
int argaddr(int n, uint64 *ip);
int argstr(int n, char *buf, int max);

// 获取进程ID
uint64 sys_getpid(void)
{
    return myproc()->pid;
}

// fork 系统调用
uint64 sys_fork(void)
{
    extern int proc_fork(void);
    return proc_fork();
}

// exit 系统调用
uint64 sys_exit(void)
{
    int status;
    if(argint(0, &status) < 0)
        return -1;
    extern void proc_exit(int);
    proc_exit(status);
    return 0;  // 不会返回
}

// wait 系统调用
uint64 sys_wait(void)
{
    uint64 addr;
    if(argaddr(0, &addr) < 0)
        return -1;
    extern int proc_wait(uint64);
    return proc_wait(addr);
}

// sleep 系统调用
uint64 sys_sleep(void)
{
    int n;
    unsigned long long ticks0;
    
    if(argint(0, &n) < 0)
        return -1;
    
    extern unsigned long long timer_ticks(void);
    extern void proc_sleep(void*, spinlock_t*);
    extern spinlock_t ticks_lock;
    
    ticks0 = timer_ticks();
    while(timer_ticks() - ticks0 < (unsigned long long)n) {
        if(myproc()->state == RUNNING) {
            proc_sleep(&ticks_lock, &ticks_lock);
        }
    }
    return 0;
}

// print 系统调用
uint64 sys_print(void)
{
    char buf[256];
    uint64 addr;
    
    if(argaddr(0, &addr) < 0)
        return -1;
    
    proc_t *p = myproc();
    if(fetchstr(p->pgtbl, addr, buf, sizeof(buf)) < 0)
        return -1;
    
    printf("%s", buf);
    return 0;
}

// kill 系统调用（暂时返回错误，后续实现）
uint64 sys_kill(void)
{
    int pid;
    if(argint(0, &pid) < 0)
        return -1;
    // TODO: 实现 kill
    printf("sys_kill: not implemented yet\n");
    return -1;
}

// sbrk 系统调用（暂时返回错误，后续实现）
uint64 sys_sbrk(void)
{
    int n;
    if(argint(0, &n) < 0)
        return -1;
    // TODO: 实现 sbrk
    printf("sys_sbrk: not implemented yet\n");
    return -1;
}

// mmap 系统调用（简化实现）
uint64 sys_mmap(void)
{
    uint64 addr, len;
    if(argaddr(0, &addr) < 0)
        return -1;
    if(argaddr(1, &len) < 0)
        return -1;
    // 简化实现：直接返回地址（实际应该分配内存）
    // TODO: 实现真正的mmap
    return addr;
}

// brk 系统调用（简化实现，复用sbrk逻辑）
uint64 sys_brk(void)
{
    uint64 addr;
    if(argaddr(0, &addr) < 0)
        return -1;
    proc_t *p = myproc();
    if(addr == 0) {
        // 获取当前堆顶
        return p->heap_top;
    } else {
        // 设置新的堆顶
        p->heap_top = addr;
        return addr;
    }
}

