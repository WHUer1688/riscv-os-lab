#include "riscv.h"
#include "common.h"
#include "dev/timer.h"

// 每核 4KB 栈（符号被 entry.S 用来设置 sp）
__attribute__ ((aligned (16))) uint8 CPU_stack[4096 * NCPU];

int main(void);
extern void timer_vector(void);

// ---- CLINT: 用 MSIP 唤醒其他核 ----
#define CLINT_BASE        0x02000000UL
#define CLINT_MSIP(h)     (CLINT_BASE + 4*(h))
static inline void wake_hart(int h) { *(volatile uint32*)CLINT_MSIP(h) = 1; }

// ---- 16550 早期串口打点（不依赖驱动/printf）----
static inline void early_putc(char c) {
  volatile uint8 *uart = (volatile uint8*)0x10000000UL; // UART0
  while ((uart[5] & 0x20) == 0) /* LSR.THRE */ ;
  uart[0] = (uint8)c;                                     // THR
}

void start(void)
{
  // 0) 允许 S 态访问所有物理地址（PMP 必须在 mret 前设置好）
  w_pmpaddr0(~0ULL);          // NAPOT: 全开
  w_pmpcfg0(0x0F);            // R|W|X + A=NAPOT

  // 1) mret 目标 = S 态的 main
  w_mepc((uint64)main);
  uint64 mstatus = r_mstatus();
  mstatus = (mstatus & ~MSTATUS_MPP_MASK) | MSTATUS_MPP_S;  // 下一态 S
  w_mstatus(mstatus);

  // 2) 中断委托：把 SSIP/SEIP 委托给 S，MTIP 保留在 M
  w_mideleg((1ULL << 1) | (1ULL << 9)); // SSIP, SEIP

  // 3) M 态定时器入口 + 允许 M 态计时器中断
  w_mtvec((uint64)timer_vector);
  w_mie(r_mie() | MIE_MTIE);
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // 4) 允许 S 态读 time/cycle/instret（可选）
  w_mcounteren(0x7);

  // 5) tp=hartid，唤醒其他核
  int id = (int)r_mhartid();
  w_tp(id);
  if (id == 0) { wake_hart(1); wake_hart(2); }

  // ===== 在 mret 之前加：让 S 态能接中断，且有第一个时钟事件 =====
  extern void kernel_vector(void);
  extern void timer_vector(void);

  // S 态陷阱入口
  w_stvec((uint64)kernel_vector);

  // 打开 S 态全局中断 SSTATUS_SIE（确保回到 S 后能响应该 SSIP）
  w_sstatus(r_sstatus() | SSTATUS_SIE);

  // 机器态定时器向量 + 允许 MTIP
  w_mtvec((uint64)timer_vector);
  w_mie(r_mie() | MIE_MTIE);
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // 预置第一次时钟：mtimecmp = mtime + INTERVAL （每核各自定时）
  volatile uint64 *mtime     = (uint64 *)0x0200BFF8ULL;
  volatile uint64 *mtimecmp0 = (uint64 *)(0x02004000ULL + 8 * r_mhartid());
  *mtimecmp0 = *mtime + INTERVAL;  // INTERVAL 可被 make 传入


  asm volatile("mret");
}
