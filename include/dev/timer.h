#pragma once
#include "common.h"

void   timer_init(void);
void   timer_inithart(void);
void   timer_ack(void);
unsigned long long timer_ticks(void);
void   timer_on_tick(void);

// 允许用 make 传参覆盖：make qemu INTERVAL=200000
#ifndef INTERVAL
#define INTERVAL 1000000ULL   // 约 0.1s（QEMU timebase 通常 10MHz）
#endif
