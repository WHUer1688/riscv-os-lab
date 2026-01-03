#include "syscall.h"
#include "proc/proc.h"
#include "fs/file.h"
#include "lib/print.h"
#include "mem/str.h"
#include "mem/kvm.h"

// 声明参数获取函数（在 syscall.c 中定义）
int argint(int n, int *ip);
int argaddr(int n, uint64 *ip);
int argstr(int n, char *buf, int max);

// 分配文件描述符
static int fdalloc(struct file *f) {
    proc_t *p = myproc();
    for (int fd = 0; fd < 16; fd++) {
        if (p->ofile[fd] == NULL) {
            p->ofile[fd] = (void*)f;
            return fd;
        }
    }
    return -1;
}

// open 系统调用
uint64 sys_open(void)
{
    char path[128];
    int omode;
    if(argstr(0, path, sizeof(path)) < 0)
        return -1;
    if(argint(1, &omode) < 0)
        return -1;
    
    struct file *f = file_open(path, omode);
    if (f == NULL) {
        return -1;
    }
    
    int fd = fdalloc(f);
    if (fd < 0) {
        file_close(f);
        return -1;
    }
    
    return fd;
}

// close 系统调用
uint64 sys_close(void)
{
    int fd;
    if(argint(0, &fd) < 0)
        return -1;
    
    proc_t *p = myproc();
    if (fd < 0 || fd >= 16 || p->ofile[fd] == NULL) {
        return -1;
    }
    
    struct file *f = (struct file*)p->ofile[fd];
    p->ofile[fd] = NULL;
    file_close(f);
    
    return 0;
}

// read 系统调用
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
    
    proc_t *proc = myproc();
    if (fd < 0 || fd >= 16 || proc->ofile[fd] == NULL) {
        return -1;
    }
    
    struct file *f = (struct file*)proc->ofile[fd];
    
    // 分配临时缓冲区
    char buf[512];
    int tot = 0;
    while (tot < n) {
        int m = n - tot;
        if (m > sizeof(buf)) {
            m = sizeof(buf);
        }
        int r = file_read(f, buf, m);
        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            break;
        }
        // 将数据复制到用户空间
        if (copyout(proc->pgtbl, p + tot, buf, r) < 0) {
            return -1;
        }
        tot += r;
        if (r < m) {
            break;
        }
    }
    
    return tot;
}

// write 系统调用
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
    
    proc_t *proc = myproc();
    if (fd < 0 || fd >= 16 || proc->ofile[fd] == NULL) {
        return -1;
    }
    
    struct file *f = (struct file*)proc->ofile[fd];
    
    // 从用户空间读取数据并写入文件
    char buf[512];
    int tot = 0;
    while (tot < n) {
        int m = n - tot;
        if (m > sizeof(buf)) {
            m = sizeof(buf);
        }
        if (copyin(proc->pgtbl, buf, p + tot, m) < 0) {
            return -1;
        }
        int r = file_write(f, buf, m);
        if (r < 0) {
            return -1;
        }
        tot += r;
        if (r < m) {
            break;
        }
    }
    
    return tot;
}

// exec 系统调用
uint64 sys_exec(void)
{
    char path[128];
    if(argstr(0, path, sizeof(path)) < 0)
        return -1;
    
    extern int proc_exec(const char* path);
    if (proc_exec(path) < 0) {
        return -1;
    }
    
    return 0;
}

