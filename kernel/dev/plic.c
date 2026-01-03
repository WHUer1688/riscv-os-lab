#include "riscv.h"
#include "dev/plic.h"

void plic_init(void)
{
  // 给 UART0 设置一个非零优先级（>0 才可能被分发）
  *(volatile uint32*)(PLIC_PRIORITY + UART0_IRQ*4) = 1;
  // 给 VIRTIO 设置一个非零优先级
  *(volatile uint32*)(PLIC_PRIORITY + VIRTIO_IRQ*4) = 1;
}

void plic_inithart(void)
{
  int hart = r_tp(); // 我们把 hartid 放在 tp （见 start.c）:contentReference[oaicite:7]{index=7}

  // 只在 S-mode 打开 UART0 和 VIRTIO 的中断使能
  volatile uint32 *senable = (uint32*)PLIC_SENABLE(hart);
  *senable |= (1u << UART0_IRQ);
  *senable |= (1u << VIRTIO_IRQ);

  // S-mode 阈值设为 0（允许所有优先级>0 的中断进入）
  *(volatile uint32*)PLIC_SPRIORITY(hart) = 0;
}

int plic_claim(void)
{
  int hart = r_tp();
  return *(volatile uint32*)PLIC_SCLAIM(hart);
}

void plic_complete(int irq)
{
  int hart = r_tp();
  *(volatile uint32*)PLIC_SCLAIM(hart) = irq;
}
