# kernel.ld 将_entry作为整个OS的起点放置到0x80000000处
# qemu会自动跳转到0x80000000处并开始执行
# 注意: 此时是M-mode

.section .text
.globl _entry
_entry:
        # CPU_stack 定义于start.c中
        # sp = CPU_stack + ((hartid + 1) * 4096)
        # 将sp置于当前CPU的内核栈的栈顶
        la sp, CPU_stack
        li a0, 4096
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        
        # ---- early putc: 直接把 '>' 打到 UART THR ----
        # 简化版本：直接写入，不等待LSR
        li   t1, 0x10000000      # UART_BASE
        li   t0, '>'
        sb   t0, 0(t1)          # 写 THR (offset 0)
        # ----------------------------------------------
        
        # 跳转到start
        call fake_timer_loop   # 斜招：直接进入假计时器，不再返回
        call start
spin:
        j spin

# ===== 假计时器循环（不依赖中断/printf/驱动）=====
# 支持 make 传参： make -B kernel INTERVAL=200000
#ifndef INTERVAL
#define INTERVAL 1000000
#endif

.equ UART0,      0x10000000
.equ UART_LSR,   5      # Line Status Register
.equ UART_THR,   0      # Transmit Holding Register
.equ UART_LSR_THRE, 0x20
.equ UART_LSR_DR,   0x01

.section .rodata
ticks_prefix:
    .asciz "ticks=0x"

.text
.globl fake_timer_loop
fake_timer_loop:
    # 仅让 hart 0 进入循环，其他核休眠
    csrr a0, mhartid
    bnez a0, .Lidle
    
    # hart0 继续
    li   s0, 0                 # s0 = ticks

.Lloop:
    # 1) 纯忙等（INTERVAL 越小越快）
    li   t0, INTERVAL
1:  addi t0, t0, -1
    bnez t0, 1b
    
    # 2) 输出 'T'
    li   a0, 'T'
    call putc_imm
    
    # ticks++
    addi s0, s0, 1
    
    # 3) 输出 "\nticks=0x"
    li   a0, '\n'
    call putc_imm
    la   a0, ticks_prefix
    call puts_imm
    
    # 4) 输出 16 个十六进制位的 ticks
    mv   t2, s0
    li   t3, 60                # 从高位 nibble 开始
2:  srl  t4, t2, t3            # 取 nibble
    andi t4, t4, 0xF
    li   t5, 10
    blt  t4, t5, 3f
    addi t4, t4, -10
    addi t4, t4, 'A'
    j    4f
3:  addi t4, t4, '0'
4:  mv   a0, t4
    call putc_imm
    addi t3, t3, -4
    bgez t3, 2b
    
    # 5) 回车换行
    li   a0, '\n'
    call putc_imm
    
    # 6) 轮询并回显：把 RX FIFO 里所有字节都刷出来
    li   t0, UART0
1:  lb   t1, UART_LSR(t0)
    andi t1, t1, UART_LSR_DR     # Data Ready?
    beqz t1, .Lloop
    lb   a0, 0(t0)               # RBR 在偏移 0
    call putc_imm
    j    1b

.Lidle:
    wfi
    j .Lidle

# ---- 低级输出：直接访问 16550 ----
# void putc_imm(a0=char)
.globl putc_imm
putc_imm:
    addi sp, sp, -16
    sd   ra, 8(sp)
    li   t0, UART0
5:  lb   t1, UART_LSR(t0)
    andi t1, t1, UART_LSR_THRE
    beqz t1, 5b
    sb   a0, UART_THR(t0)
    ld   ra, 8(sp)
    addi sp, sp, 16
    ret

# void puts_imm(a0=addr)
.globl puts_imm
puts_imm:
    addi sp, sp, -16
    sd   ra, 8(sp)
    mv   t6, a0                # 保存字符串地址到 t6
6:  lbu  t0, 0(t6)             # 从 t6 读取字符
    beqz t0, 7f
    mv   a0, t0                # 字符作为参数
    call putc_imm
    addi t6, t6, 1             # 递增地址指针
    j    6b
7:  ld   ra, 8(sp)
    addi sp, sp, 16
    ret
