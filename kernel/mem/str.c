#include "mem/str.h"

// 计算字符串长度
int strlen(const char *s)
{
    int n;
    for(n = 0; s[n]; n++)
        ;
    return n;
}

// 复制字符串
char* strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while((*dst++ = *src++) != 0)
        ;
    return ret;
}

// 比较字符串
int strcmp(const char *p, const char *q)
{
    while(*p && *p == *q)
        p++, q++;
    return (unsigned char)*p - (unsigned char)*q;
}

// 设置内存
void *memset(void *dst, int c, uint64 n)
{
    char *cdst = (char *) dst;
    int i;
    for(i = 0; i < n; i++){
        cdst[i] = c;
    }
    return dst;
}

// 移动内存
void *memmove(void *dst, const void *src, uint64 n)
{
    char *cdst = (char *) dst;
    const char *csrc = (const char *) src;
    if(cdst < csrc){
        for(int i = 0; i < n; i++){
            cdst[i] = csrc[i];
        }
    } else {
        for(int i = n-1; i >= 0; i--){
            cdst[i] = csrc[i];
        }
    }
    return dst;
}

// 复制内存
void *memcpy(void *dst, const void *src, uint64 n)
{
    return memmove(dst, src, n);
}
