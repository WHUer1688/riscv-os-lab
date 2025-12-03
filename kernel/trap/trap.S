    .section .text
    .globl kernel_vector
    .globl timer_vector

// ============= S-mode trap 向量：保存现场 -> C 处理 -> 恢复现场 -> sret
kernel_vector:
    addi sp, sp, -(34*8)
    // 保存通用寄存器
    sd ra,  0(sp);  sd sp,  8(sp);  sd gp, 16(sp);  sd tp, 24(sp)
    sd t0, 32(sp);  sd t1, 40(sp);  sd t2, 48(sp)
    sd s0, 56(sp);  sd s1, 64(sp)
    sd a0, 72(sp);  sd a1, 80(sp);  sd a2, 88(sp);  sd a3, 96(sp)
    sd a4,104(sp);  sd a5,112(sp);  sd a6,120(sp);  sd a7,128(sp)
    sd s2,136(sp);  sd s3,144(sp);  sd s4,152(sp);  sd s5,160(sp)
    sd s6,168(sp);  sd s7,176(sp);  sd s8,184(sp);  sd s9,192(sp)
    sd s10,200(sp); sd s11,208(sp)
    sd t3,216(sp);  sd t4,224(sp);  sd t5,232(sp);  sd t6,240(sp)

    // 保存关键信息到栈尾（便于 C 侧读取）
    csrr t0, sepc
    csrr t1, sstatus
    csrr t2, scause
    csrr t3, stval
    sd t0, 248(sp); sd t1, 256(sp); sd t2, 264(sp); sd t3, 272(sp)

    // a0 = &trapframe
    mv a0, sp
    call trap_kernel_handler

    // 恢复现场
    ld ra,  0(sp);  ld sp,  8(sp);  ld gp, 16(sp);  ld tp, 24(sp)
    ld t0, 32(sp);  ld t1, 40(sp);  ld t2, 48(sp)
    ld s0, 56(sp);  ld s1, 64(sp)
    ld a0, 72(sp);  ld a1, 80(sp);  ld a2, 88(sp);  ld a3, 96(sp)
    ld a4,104(sp);  ld a5,112(sp);  ld a6,120(sp);  ld a7,128(sp)
    ld s2,136(sp);  ld s3,144(sp);  ld s4,152(sp);  ld s5,160(sp)
    ld s6,168(sp);  ld s7,176(sp);  ld s8,184(sp);  ld s9,192(sp)
    ld s10,200(sp); ld s11,208(sp)
    ld t3,216(sp);  ld t4,224(sp);  ld t5,232(sp);  ld t6,240(sp)

    // trap_kernel_handler 可选择修改 sepc，这里按其写回
    ld t0, 248(sp)
    csrw sepc, t0
    addi sp, sp, (34*8)
    sret

// ============= M-mode 定时器中断向量：重装 mtimecmp + 置 SSIP -> mret
    .equ CLINT_MTIME,      0x0200BFF8
    .equ CLINT_MTIMECMP,   0x02004000
    .equ TICK_INTERVAL,    100000

timer_vector:
    addi sp, sp, -16
    sd a0, 8(sp)

    // 计算当前 hart 的 mtimecmp 地址：CLINT_MTIMECMP + 8*hartid
    csrr a0, mhartid
    slli a0, a0, 3
    li   t0, CLINT_MTIMECMP
    add  t0, t0, a0

    // 读取 mtime + INTERVAL 写回 mtimecmp
    li   t1, CLINT_MTIME
    ld   t2, 0(t1)
    li   t3, TICK_INTERVAL
    add  t2, t2, t3
    sd   t2, 0(t0)

    // 置位 S-mode 软件中断 SSIP（PDF 第 15 页描述的“在 M 里触发 S 软件中断”）
    li   t0, 2
    csrs sip, t0

    ld a0, 8(sp)
    addi sp, sp, 16
    mret
