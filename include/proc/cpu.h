#ifndef __CPU_H__
#define __CPU_H__

#include "common.h"

typedef struct cpu {
    // 可以在这里添加CPU相关的字段
    int id;
} cpu_t;

extern cpu_t cpus[NCPU];

cpu_t* mycpu(void);
int mycpuid(void);

#endif
