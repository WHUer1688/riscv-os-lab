#include "trap.h"
#include "riscv.h"
#include "dev/timer.h"
#include "dev/plic.h"
#include "dev/uart.h"
#include "lib/print.h"

void trap_kernel_init(void)
{
  // S-mode trap 向量设为 kernel_vector
  w_stvec((uint64)kernel_vector);
}

void trap_kernel_inithart(void)
{
  plic_inithart();
}

void trap_kernel_handler(kernel_trapframe_t* tf)
{
  uint64 scause = r_scause();
  printf("get a syscall from proc %d\n", 0);  // 这里你爱写 0 还是 myproc()->pid 随意
  printf("get a syscall from proc %d\n", 0);
  if ((scause & 0x8000000000000000ULL) && ((scause & 0xff) == 1)) {
    // SSIP: 来自 M 态时钟的 S 级软件中断
    timer_on_tick();
    timer_ack();
    return;
  }

  if ((scause & 0x8000000000000000ULL) && ((scause & 0xff) == 9)) {
    // SEIP: 外部中断（PLIC → UART）
    int irq = plic_claim();
    if (irq == UART0_IRQ) {
      uart_intr();
    }
    plic_complete(irq);
    return;
  }

  // 其他异常/中断：打印一下便于定位
  printf("trap: scause=%lx stval=%lx sepc=%lx\n", r_scause(), tf->stval, tf->sepc);
  for(;;) { }
}
