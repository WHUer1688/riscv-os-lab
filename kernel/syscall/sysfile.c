#include "syscall.h"
#include "proc/proc.h"
#include "lib/print.h"
#include "mem/str.h"

// 声明参数获取函数（在 syscall.c 中定义）
int argint(int n, int *ip);
int argaddr(int n, uint64 *ip);
int argstr(int n, char *buf, int max);

// open 系统调用（暂时返回错误，后续实现）
uint64 sys_open(void)
{
    char path[128];
    int omode;
    if(argstr(0, path, sizeof(path)) < 0)
        return -1;
    if(argint(1, &omode) < 0)
        return -1;
    // TODO: 实现 open
    printf("sys_open: not implemented yet\n");
    return -1;
}

// close 系统调用（暂时返回错误，后续实现）
uint64 sys_close(void)
{
    int fd;
    if(argint(0, &fd) < 0)
        return -1;
    // TODO: 实现 close
    printf("sys_close: not implemented yet\n");
    return -1;
}

// read 系统调用（暂时返回错误，后续实现）
uint64 sys_read(void)
{
    int fd, n;
    uint64 p;
    if(argint(0, &fd) < 0)
        return -1;
    if(argaddr(1, &p) < 0)
        return -1;
    if(argint(2, &n) < 0)
        return -1;
    // TODO: 实现 read
    printf("sys_read: not implemented yet\n");
    return -1;
}

// write 系统调用（暂时返回错误，后续实现）
uint64 sys_write(void)
{
    int fd, n;
    uint64 p;
    if(argint(0, &fd) < 0)
        return -1;
    if(argaddr(1, &p) < 0)
        return -1;
    if(argint(2, &n) < 0)
        return -1;
    // TODO: 实现 write
    printf("sys_write: not implemented yet\n");
    return -1;
}

