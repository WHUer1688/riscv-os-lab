#pragma once
#include <stdint.h>
#include "riscv.h"
#include "memlayout.h"

#define PLIC_PRIORITY           (PLIC_BASE + 0x000000)
#define PLIC_PENDING            (PLIC_BASE + 0x001000)
#define PLIC_MENABLE(h)         (PLIC_BASE + 0x002000 + (h)*0x100)
#define PLIC_SENABLE(h)         (PLIC_BASE + 0x002080 + (h)*0x100)
#define PLIC_MPRIORITY(h)       (PLIC_BASE + 0x200000 + (h)*0x2000)
#define PLIC_SPRIORITY(h)       (PLIC_BASE + 0x201000 + (h)*0x2000)
#define PLIC_MCLAIM(h)          (PLIC_BASE + 0x200004 + (h)*0x2000)
#define PLIC_SCLAIM(h)          (PLIC_BASE + 0x201004 + (h)*0x2000)

#define UART0_IRQ 10

void plic_init(void);
void plic_inithart(void);
int  plic_claim(void);
void plic_complete(int irq);
