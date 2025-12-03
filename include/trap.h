#pragma once
#include <stdint.h>
#include "riscv.h"

typedef struct trapframe {
  uint64 ra, sp, gp, tp;
  uint64 t0,t1,t2;
  uint64 s0,s1;
  uint64 a0,a1,a2,a3,a4,a5,a6,a7;
  uint64 s2,s3,s4,s5,s6,s7,s8,s9,s10,s11;
  uint64 t3,t4,t5,t6;

  uint64 sepc;
  uint64 sstatus;
  uint64 scause;
  uint64 stval;
} trapframe_t;

void trap_kernel_init(void);
void trap_kernel_inithart(void);
void trap_kernel_handler(trapframe_t* tf);

void kernel_vector(void);
void timer_vector(void);
