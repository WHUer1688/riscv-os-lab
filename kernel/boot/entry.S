# kernel.ld 将_entry作为整个OS的起点放置到0x80000000处
# qemu会自动跳转到0x80000000处并开始执行
# 注意: 此时是M-mode

.section .text
.globl _entry
_entry:
        la sp, CPU_stack
        li a0, 4096
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
    
        li   t1, 0x10000000
        li   t0, '>'
        sb   t0, 0(t1)         
        
        call timer_loop  
        call start
spin:
        j spin


#ifndef INTERVAL
#define INTERVAL 1000000
#endif

.equ UART0,      0x10000000
.equ UART_LSR,   5  
.equ UART_THR,   0     
.equ UART_LSR_THRE, 0x20
.equ UART_LSR_DR,   0x01

.section .rodata
ticks_prefix:
    .asciz "ticks=0x"

.text
.globl fake_timer_loop
timer_loop:
    csrr a0, mhartid
    bnez a0, .Lidle
    
    # hart0 继续
    li   s0, 0        

.Lloop:
    li   t0, INTERVAL
1:  addi t0, t0, -1
    bnez t0, 1b

    li   a0, 'T'
    call putc_imm
    
    addi s0, s0, 1
    
    li   a0, '\n'
    call putc_imm
    la   a0, ticks_prefix
    call puts_imm
    
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
    
    li   a0, '\n'
    call putc_imm

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
