#include "proc/cpu.h"
#include "riscv.h"

cpu_t cpus[NCPU];

cpu_t* mycpu(void)
{
    int id = r_tp();
    return &cpus[id];
}

int mycpuid(void) 
{
    return r_tp();
}
