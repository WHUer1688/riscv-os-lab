// ===== 超简版 FAKE TIMER：不依赖中断/trap，纯忙等+UART MMIO =====
#include "riscv.h"
#include "common.h"

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
  // 只让 cpu0 打印（避免多核重复输出）
  int cpuid = (int)r_tp();
  if (cpuid != 0) {
    for(;;) { 
      __asm__ volatile("wfi"); 
    }
  }

  uart0_puts_imm("\n[FAKE TIMER MODE]\n");

  uint64 ticks = 0;
  for (;;) {
    // 1) "滴答延时" —— 纯忙等
    soft_delay(INTERVAL);

    // 2) 滴答应答：输出 'T'
    uart0_putc_imm('T');

    // 3) ticks 输出（每次都打一行）
    uart0_puts_imm("\nticks=");
    uart0_putu64_imm(++ticks);
    uart0_putc_imm('\n');

    // 4) 键盘输入回显（轮询兜底）
    int ch;
    while ((ch = uart0_try_getc_imm()) >= 0) {
      uart0_putc_imm((char)ch);
    }
  }
}
