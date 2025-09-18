#include "riscv.h"

__attribute__ ((aligned (16))) uint8 CPU_stack[4096 * NCPU];

void main();

// CLINT (Core Local Interruptor) 基地址
#define CLINT_BASE 0x2000000L
#define CLINT_MSIP(hartid) (CLINT_BASE + 4*(hartid))

// 唤醒指定的hart
void wakeup_hart(int hartid)
{
    // 设置MSIP位来唤醒hart
    *(volatile uint32*)CLINT_MSIP(hartid) = 1;
}

void start()
{
    // 设置M模式下的特权级别为Supervisor模式，用于mret返回
    uint64 x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    // 设置M模式的异常程序计数器为main函数地址
    w_mepc((uint64)main);

    // 暂时禁用分页
    w_satp(0);

    // 将所有中断和异常委托给Supervisor模式
    w_medeleg(0xffff);
    w_mideleg(0xffff);
    w_sie(r_sie() | SIE_SEIE | SIE_STIE);

    // 配置物理内存保护，给Supervisor模式访问所有物理内存的权限
    w_pmpaddr0(0x3fffffffffffffull);
    w_pmpcfg0(0xf);

    // 将当前CPU的hartid保存在tp寄存器中
    int id = r_mhartid();
    w_tp(id);

    // 如果是hart0，唤醒其他hart
    if(id == 0) {
        // 唤醒hart1和hart2
        wakeup_hart(1);
        wakeup_hart(2);
    }

    // 切换到Supervisor模式并跳转到main函数
    asm volatile("mret");
}