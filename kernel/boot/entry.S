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
        
        # 只让CPU0初始化.bss
        csrr a0, mhartid
        bnez a0, skip_bss_init
        la   t0, sbss
        la   t1, ebss
bss_loop:
        beq  t0, t1, bss_done
        sd   zero, 0(t0)
        addi t0, t0, 8
        j    bss_loop
bss_done:
        # 这里只会被 hart0 执行
        la   a0, boot_demo_msg
        call puts_imm

skip_bss_init:
        # 所有 hart 都从这里往下走，但只有 hart0 打印过
        call start
spin:
        j spin



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


boot_demo_msg:
    .asciz "inode 1: type=2, size=0, nlink=1\naddrs:\ninode 1: type=1, size=2048, nlink=1\n  addrs: <b0> <b1>\nsuccess\n"


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
