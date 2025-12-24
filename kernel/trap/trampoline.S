.section trampsec
.globl trampoline
.globl user_vector
.globl user_return

trampoline:
# user_vector: 用户态trap入口
# 当用户态发生trap时，硬件会跳转到这里
# 需要保存用户寄存器到trapframe，然后调用trap_user_handler
user_vector:
    # 保存用户态寄存器到trapframe
    # trapframe地址在sscratch中（由内核设置）
    csrrw a0, sscratch, a0
    
    # 保存所有通用寄存器
    sd ra, 40(a0)
    sd sp, 48(a0)
    sd gp, 56(a0)
    sd tp, 64(a0)
    sd t0, 72(a0)
    sd t1, 80(a0)
    sd t2, 88(a0)
    sd s0, 96(a0)
    sd s1, 104(a0)
    sd a1, 120(a0)
    sd a2, 128(a0)
    sd a3, 136(a0)
    sd a4, 144(a0)
    sd a5, 152(a0)
    sd a6, 160(a0)
    sd a7, 168(a0)
    sd s2, 176(a0)
    sd s3, 184(a0)
    sd s4, 192(a0)
    sd s5, 200(a0)
    sd s6, 208(a0)
    sd s7, 216(a0)
    sd s8, 224(a0)
    sd s9, 232(a0)
    sd s10, 240(a0)
    sd s11, 248(a0)
    sd t3, 256(a0)
    sd t4, 264(a0)
    sd t5, 272(a0)
    sd t6, 280(a0)
    
    # 保存a0（之前交换到sscratch了）
    csrr t0, sscratch
    sd t0, 112(a0)
    
    # 保存CSR寄存器
    csrr t0, sepc
    sd t0, 24(a0)
    # 注意：sstatus不需要保存到trapframe，因为我们可以从CSR读取
    
    # 切换到内核栈
    ld sp, 8(a0)  # kernel_sp
    
    # 切换到内核页表
    ld t0, 0(a0)  # kernel_satp
    csrw satp, t0
    sfence.vma zero, zero
    
    # 设置内核hartid
    ld tp, 32(a0)  # kernel_hartid
    # tp是通用寄存器，直接使用即可
    
    # 调用trap_user_handler
    ld t0, 16(a0)  # kernel_trap (trap_user_handler地址)
    jalr t0
    
    # trap_user_handler返回后，跳转到user_return
    j user_return

# user_return: 从内核返回用户态
# 恢复trapframe中的寄存器，然后sret
user_return:
    # 此时a0指向trapframe（由trap_user_return设置）
    # 需要恢复所有寄存器，然后sret
    
    # 恢复CSR寄存器
    ld t0, 24(a0)  # epc
    csrw sepc, t0
    
    # 恢复sstatus（需要清除SPP，设置SPIE）
    # SSTATUS_SPP = 1 << 8, SSTATUS_SPIE = 1 << 5
    # 注意：sstatus已经在trap_user_return中设置好了
    # 这里只需要确保SPP=0，SPIE=1（已经在trap_user_return中设置）
    
    # 恢复通用寄存器（除了a0和sp，它们需要特殊处理）
    ld ra, 40(a0)
    ld gp, 56(a0)
    ld tp, 64(a0)
    ld t0, 72(a0)
    ld t1, 80(a0)
    ld t2, 88(a0)
    ld s0, 96(a0)
    ld s1, 104(a0)
    ld a1, 120(a0)
    ld a2, 128(a0)
    ld a3, 136(a0)
    ld a4, 144(a0)
    ld a5, 152(a0)
    ld a6, 160(a0)
    ld a7, 168(a0)
    ld s2, 176(a0)
    ld s3, 184(a0)
    ld s4, 192(a0)
    ld s5, 200(a0)
    ld s6, 208(a0)
    ld s7, 216(a0)
    ld s8, 224(a0)
    ld s9, 232(a0)
    ld s10, 240(a0)
    ld s11, 248(a0)
    ld t3, 256(a0)
    ld t4, 264(a0)
    ld t5, 272(a0)
    ld t6, 280(a0)
    
    # 恢复sp和a0（最后恢复，因为a0指向trapframe）
    ld sp, 48(a0)
    ld a0, 112(a0)
    
    # 返回用户态
    sret

