#include "dev/timer.h"
#include "riscv.h"

// ---- CLINT ----
#define CLINT_MTIMECMP(h) (0x02004000UL + 8*(h))
#define CLINT_MTIME       (0x0200BFF8UL)

// ---- 16550A UART MMIO（QEMU virt）----
static inline void uart0_putc_imm(char c) {
  volatile unsigned char *uart = (volatile unsigned char*)0x10000000UL;
  while ((uart[5] & 0x20) == 0) { }  // LSR.THRE
  uart[0] = (unsigned char)c;        // THR
}

static void uart0_puts_imm(const char *s) {
  while (*s) uart0_putc_imm(*s++);
}

static void uart0_putu64_imm(unsigned long long x) {
  char buf[32]; int i = 0;
  if (x == 0) { uart0_putc_imm('0'); return; }
  while (x) { buf[i++] = '0' + (x % 10); x /= 10; }
  while (i--) uart0_putc_imm(buf[i]);
}

#ifndef INTERVAL
#define INTERVAL 1000000ULL
#endif

static volatile unsigned long long ticks_v = 0;

static inline unsigned long long mtime_read(void) {
  return *(volatile unsigned long long*)CLINT_MTIME;
}

static inline void set_mtimecmp(int hart, unsigned long long when) {
  *(volatile unsigned long long*)CLINT_MTIMECMP(hart) = when;
}

void timer_init(void) {
  int id = (int)r_tp();
  set_mtimecmp(id, mtime_read() + INTERVAL);
}

void timer_inithart(void) { timer_init(); }

void timer_ack(void) { w_sip(r_sip() & ~SIP_SSIP); }

unsigned long long timer_ticks(void) { return ticks_v; }

// —— 核心：每次时钟滴答都直接打印 ——
// （满足：1. 输出 'T'；2. 打印 ticks；3. 重装下一次定时）
void timer_on_tick(void) {
  ticks_v++;

  // 1) 时钟滴答测试：输出 'T' 字符
  uart0_putc_imm('T');

  // 2) 时钟快慢测试：输出 ticks（一行）
  uart0_putc_imm('\n');
  uart0_puts_imm("ticks=");
  uart0_putu64_imm(ticks_v);
  uart0_putc_imm('\n');

  // 3) 安排下一次定时器中断
  int id = (int)r_tp();
  set_mtimecmp(id, mtime_read() + INTERVAL);
}
