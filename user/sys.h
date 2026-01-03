#ifndef __SYS_H__
#define __SYS_H__

#include "syscall.h"
#include <stdarg.h>

// 通用系统调用函数（支持可变参数）
static inline long long syscall(long long n, ...) {
    va_list ap;
    va_start(ap, n);
    long long a0 = va_arg(ap, long long);
    long long a1 = va_arg(ap, long long);
    long long a2 = va_arg(ap, long long);
    long long a3 = va_arg(ap, long long);
    long long a4 = va_arg(ap, long long);
    long long a5 = va_arg(ap, long long);
    va_end(ap);
    
    long long ret;
    __asm__ volatile (
        "mv a7, %1\n"
        "mv a0, %2\n"
        "mv a1, %3\n"
        "mv a2, %4\n"
        "mv a3, %5\n"
        "mv a4, %6\n"
        "mv a5, %7\n"
        "ecall\n"
        "mv %0, a0"
        : "=r" (ret)
        : "r" (n), "r" (a0), "r" (a1), "r" (a2), "r" (a3), "r" (a4), "r" (a5)
        : "a0", "a1", "a2", "a3", "a4", "a5", "a7"
    );
    return ret;
}

#endif // __SYS_H__

