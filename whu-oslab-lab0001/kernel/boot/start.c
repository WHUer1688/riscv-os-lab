#include "riscv.h"

__attribute__ ((aligned (16))) uint8 CPU_stack[4096 * NCPU];

void main();

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

    // 切换到Supervisor模式并跳转到main函数
    asm volatile("mret");
}