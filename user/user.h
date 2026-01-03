#ifndef __USER_H__
#define __USER_H__

// 用户态系统调用函数声明

// 进程相关
int fork(void);
void exit(int) __attribute__((noreturn));
int wait(int*);
int kill(int pid);
int getpid(void);

// 文件相关
int open(const char*, int);
int close(int);
int read(int, void*, int);
int write(int, const void*, int);

// 内存相关
void* sbrk(int);

#endif // __USER_H__

