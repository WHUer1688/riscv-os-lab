// ===== Lab4: 创建proczero并切换到用户态 =====
#include "riscv.h"
#include "common.h"
#include "mem/pmem.h"
#include "mem/kvm.h"
#include "trap.h"
#include "proc/proc.h"
#include "lib/print.h"

// 默认"间隔规模"。也可以用 make 传：make qemu INTERVAL=200000
#ifndef INTERVAL
#define INTERVAL 1000000ULL
#endif

// UART0 (16550A) MMIO（QEMU virt）
static inline void uart0_putc_imm(char c) {
  volatile unsigned char *uart = (volatile unsigned char *)0x10000000UL;
  // 等待 THR 空
  while ((uart[5] & 0x20) == 0) {}
  uart[0] = (unsigned char)c;  // THR
}

static inline int uart0_try_getc_imm(void) {
  volatile unsigned char *uart = (volatile unsigned char *)0x10000000UL;
  if (uart[5] & 0x01) return uart[0];  // LSR.DR
  return -1;
}

static inline void uart0_puts_imm(const char *s) {
  while (*s) uart0_putc_imm(*s++);
}

static inline void uart0_putu64_imm(uint64 x) {
  char buf[32]; int i = 0;
  if (x == 0) { uart0_putc_imm('0'); return; }
  while (x) { buf[i++] = '0' + (x % 10); x /= 10; }
  while (i--) uart0_putc_imm(buf[i]);
}

// 纯软件忙等，INTERVAL 越大，等待越久
static inline void soft_delay(uint64 n) {
  // 防止被优化掉
  for (volatile uint64 i = 0; i < n; ++i) { __asm__ volatile(""); }
}

int main(void) {
  int cpuid = (int)r_tp();
  
  // 只让CPU0进行初始化
  if (cpuid == 0) {
    // 初始化物理内存管理
    pmem_init();
    
    // 初始化内核页表
    kvm_init();
    
    // 初始化trap
    trap_kernel_init();
  }
  
  // 每个CPU初始化自己的页表和trap
  kvm_inithart();
  trap_kernel_inithart();
  
  // 其他CPU在main末尾死循环
  if (cpuid != 0) {
    for(;;) { 
      __asm__ volatile("wfi"); 
    }
  }
  
  // CPU0创建proczero并切换到用户态
  // proc_make_first会切换到proczero，不会返回
  proc_make_first();
  
  // 如果返回了（不应该发生），进入死循环
  for(;;) {
    __asm__ volatile("wfi");
  }
}
