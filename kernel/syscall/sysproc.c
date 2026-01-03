#include "syscall.h"
#include "proc/proc.h"
#include "lib/print.h"
#include "mem/str.h"

// 声明参数获取函数（在 syscall.c 中定义）
int argint(int n, int *ip);
int argaddr(int n, uint64 *ip);

// 获取进程ID
uint64 sys_getpid(void)
{
    return myproc()->pid;
}

// fork 系统调用（暂时返回错误，后续实现）
uint64 sys_fork(void)
{
    // TODO: 实现 fork
    printf("sys_fork: not implemented yet\n");
    return -1;
}

// exit 系统调用（暂时返回错误，后续实现）
uint64 sys_exit(void)
{
    int status;
    if(argint(0, &status) < 0)
        return -1;
    // TODO: 实现 exit
    printf("sys_exit: not implemented yet\n");
    return -1;
}

// wait 系统调用（暂时返回错误，后续实现）
uint64 sys_wait(void)
{
    uint64 addr;
    if(argaddr(0, &addr) < 0)
        return -1;
    // TODO: 实现 wait
    printf("sys_wait: not implemented yet\n");
    return -1;
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

