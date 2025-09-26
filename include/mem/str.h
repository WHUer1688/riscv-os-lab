#ifndef __STR_H__
#define __STR_H__

#include "common.h"

// 字符串操作函数声明
int strlen(const char *s);
char* strcpy(char *dst, const char *src);
int strcmp(const char *p, const char *q);
void *memset(void *dst, int c, uint64 n);
void *memmove(void *dst, const void *src, uint64 n);
void *memcpy(void *dst, const void *src, uint64 n);

#endif
