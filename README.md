# LAB-1: 机器启动

## 项目简介

本项目实现了一个基于RISC-V架构的多核操作系统内核，完成了机器启动、多核同步、printf输出和自旋锁等核心功能。项目参考了xv6-riscv的设计理念，实现了从汇编启动到C语言主程序的完整启动流程。

## 编译运行说明

### 环境要求
- RISC-V工具链 (riscv64-linux-gnu-gcc)
- QEMU模拟器 (支持RISC-V)
- Make构建工具
- Linux环境 (推荐Ubuntu 20.04+)

### 编译步骤
```bash
# 进入项目目录
cd whu-oslab-lab1

# 编译内核
make build

# 运行内核 (3核模式)
make qemu
```

### 运行结果
成功运行后应该看到以下输出：
```
hello os
cpu 0 is booting!
cpu 1 is booting!
cpu 2 is booting!
```

### 停止运行
在QEMU中按 `Ctrl+A` 然后按 `X` 退出

## 项目结构

```
whu-oslab-lab1/
├── include/                    # 头文件目录
│   ├── common.h               # 通用类型定义
│   ├── riscv.h                # RISC-V寄存器操作
│   ├── memlayout.h            # 内存布局定义
│   ├── uart.h                 # UART设备接口
│   ├── lib/
│   │   ├── print.h            # 打印函数声明
│   │   └── lock.h             # 自旋锁接口
│   └── proc/
│       └── cpu.h              # CPU相关定义
├── kernel/                    # 内核源码目录
│   ├── boot/                  # 启动相关代码
│   │   ├── entry.S            # 汇编启动入口
│   │   ├── start.c            # C语言启动代码
│   │   ├── main.c             # 主程序入口
│   │   └── Makefile
│   ├── dev/                   # 设备驱动
│   │   ├── uart.c             # UART串口驱动
│   │   └── Makefile
│   ├── lib/                   # 内核库函数
│   │   ├── print.c            # printf实现
│   │   ├── spinlock.c         # 自旋锁实现
│   │   └── Makefile
│   ├── proc/                  # 进程管理
│   │   ├── proc.c             # 进程相关函数
│   │   └── Makefile
│   ├── Makefile               # 内核构建文件
│   └── kernel.ld              # 链接脚本
├── Makefile                   # 主构建文件
├── common.mk                  # 通用构建规则
├── README.md                  # 项目说明
├── report.md                  # 实验报告
└── LICENSE                    # 许可证
```

## 核心功能

### ✅ 已实现功能

1. **多核启动机制**
   - 实现了从entry.S到start.c到main.c的完整启动流程
   - 支持3核并行启动 (CPU 0, 1, 2)
   - 使用CLINT MSIP机制唤醒从核心

2. **printf输出系统**
   - 支持%d, %x, %p, %s等格式符
   - 基于UART的字符输出
   - 多核安全的输出保护

3. **自旋锁同步机制**
   - 完整的自旋锁实现 (acquire/release)
   - 中断嵌套控制 (push_off/pop_off)
   - 原子操作和内存屏障

4. **多核同步测试**
   - 并行加法计算测试
   - 竞争条件验证
   - 同步机制验证

### 🔧 技术特点

- **RISC-V架构**: 支持M模式和S模式切换
- **多核支持**: 基于CLINT的核间通信
- **同步机制**: 自旋锁保护临界区
- **内存管理**: 物理内存保护配置
- **中断处理**: 嵌套中断控制机制  

