#include "syscall.h"
#include "proc/proc.h"
#include "riscv.h"
#include "lib/print.h"
#include "mem/kvm.h"
#include "mem/str.h"

// 系统调用函数声明（在 sysproc.c/sysfile.c 实现）
extern uint64 sys_fork(void);
extern uint64 sys_exit(void);
extern uint64 sys_wait(void);
extern uint64 sys_kill(void);
extern uint64 sys_getpid(void);
extern uint64 sys_sbrk(void);

extern uint64 sys_open(void);
extern uint64 sys_close(void);
extern uint64 sys_read(void);
extern uint64 sys_write(void);

// 系统调用函数指针数组
static uint64 (*syscalls[])(void) = {
    [SYS_fork]   sys_fork,
    [SYS_exit]   sys_exit,
    [SYS_wait]   sys_wait,
    [SYS_kill]   sys_kill,
    [SYS_getpid] sys_getpid,
    [SYS_sbrk]   sys_sbrk,
    [SYS_open]   sys_open,
    [SYS_close]  sys_close,
    [SYS_read]   sys_read,
    [SYS_write]  sys_write,
};

// 从 trapframe 获取第 n 个参数（原始值）
static uint64 argraw(int n)
{
    proc_t *p = myproc();
    switch(n){
    case 0: return p->tf->a0;
    case 1: return p->tf->a1;
    case 2: return p->tf->a2;
    case 3: return p->tf->a3;
    case 4: return p->tf->a4;
    case 5: return p->tf->a5;
    default: return 0;
    }
}

// 获取整数参数
int argint(int n, int *ip)
{
    *ip = (int)argraw(n);
    return 0;
}

// 获取地址参数
int argaddr(int n, uint64 *ip)
{
    *ip = argraw(n);
    return 0;
}

// 获取字符串参数
int argstr(int n, char *buf, int max)
{
    uint64 addr;
    if(argaddr(n, &addr) < 0)
        return -1;
    proc_t *p = myproc();
    return fetchstr(p->pgtbl, addr, buf, max);
}

// 系统调用分发器
void syscall(void)
{
    proc_t *p = myproc();
    int num = p->tf->a7;
    
    if(num > 0 && num < (int)(sizeof(syscalls)/sizeof(syscalls[0])) && syscalls[num]){
        p->tf->a0 = syscalls[num]();
    } else {
        printf("pid %d: unknown syscall %d\n", p->pid, num);
        p->tf->a0 = -1;
    }
}

