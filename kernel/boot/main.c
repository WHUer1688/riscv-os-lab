// ===== Lab4: 创建proczero并切换到用户态 =====
#include "riscv.h"
#include "common.h"
#include "mem/pmem.h"
#include "mem/kvm.h"
#include "trap.h"
#include "proc/proc.h"
#include "lib/print.h"
#include "dev/virtio.h"

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
  
  if (cpuid == 0) {
    // CPU0: 各种初始化
    pmem_init();
    kvm_init();
    trap_kernel_init();
    
    // 每个CPU初始化自己的页表和trap
    kvm_inithart();
    trap_kernel_inithart();
    
    // 初始化VirtIO磁盘
    virtio_init();
    
    // 初始化进程系统
    proc_init();
    
    // 关键：创建并切到用户态 proczero
    proc_make_first();
    
    // proc_make_first 会切换到用户态，不会返回
    for(;;) {}
  } else {
    // 其他核按实验要求死循环
    kvm_inithart();
    trap_kernel_inithart();
    for(;;) {}
  }
}
