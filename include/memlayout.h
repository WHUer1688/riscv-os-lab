/* memory leyout */
#ifndef __MEMLAYOUT_H__
#define __MEMLAYOUT_H__

// UART 相关
#define UART_BASE  0x10000000ul
#define UART_IRQ   10

// 内核基地址
#define KERNEL_BASE 0x80000000ul

// 内存布局相关地址
#define KERNEL_DATA 0x80000000ul
// 分配区域从内核数据段结束后开始，避免与内核代码重叠
#define ALLOC_BEGIN 0x80000000ul + 0x2000000ul   // 32MB，给内核代码和数据留足够空间
#define ALLOC_END   0x80000000ul + 0x8000000ul   // 128MB

// 内核物理页数量（减少以避免与用户区域重叠）
#define KERNEL_PAGES 512

// 硬件寄存器地址
#define CLINT_BASE 0x2000000ul
#define PLIC_BASE  0x0c000000ul

#endif