#ifndef __CPU_H__
#define __CPU_H__

#include "common.h"
#include "proc/proc.h"

typedef struct cpu {
    int id;
    proc_t *proc;        // CPU当前运行的进程（用户进程）
    context_t ctx;       // CPU的上下文
} cpu_t;

extern cpu_t cpus[NCPU];

cpu_t* mycpu(void);
int mycpuid(void);

#endif
