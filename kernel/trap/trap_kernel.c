#include "trap.h"
#include "riscv.h"
#include "dev/timer.h"
#include "dev/plic.h"
#include "dev/uart.h"
#include "dev/virtio.h"
#include "lib/print.h"
#include "proc/proc.h"
#include "lib/lock.h"

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
  
  if ((scause & 0x8000000000000000ULL) && ((scause & 0xff) == 1)) {
    // SSIP: 来自 M 态时钟的 S 级软件中断
    extern void timer_on_tick(void);
    extern void timer_ack(void);
    extern void proc_yield(void);
    
    timer_on_tick();
    timer_ack();
    
    // 内核态也可能被timer打断，需要处理时间片
    proc_t *p = myproc();
    if(p && p->state == RUNNING) {
      spinlock_acquire(&p->lk);
      p->time_slice--;
      if(p->time_slice <= 0) {
        p->time_slice = 10;  // DEFAULT_SLICE
        spinlock_release(&p->lk);
        proc_yield();
      } else {
        spinlock_release(&p->lk);
      }
    }
    return;
  }

  if ((scause & 0x8000000000000000ULL) && ((scause & 0xff) == 9)) {
    // SEIP: 外部中断（PLIC → UART/VIRTIO）
    int irq = plic_claim();
    if (irq == UART0_IRQ) {
      uart_intr();
    } else if (irq == VIRTIO_IRQ) {
      virtio_intr();
    }
    plic_complete(irq);
    return;
  }

  // 其他异常/中断：打印一下便于定位
  printf("trap: scause=%lx stval=%lx sepc=%lx\n", r_scause(), tf->stval, tf->sepc);
  for(;;) { }
}
