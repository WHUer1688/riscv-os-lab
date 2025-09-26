# LAB-2: 内存管理

## 项目简介

本项目在LAB-1的基础上，实现了完整的物理内存管理系统，包括内存分配、释放、页表管理和虚拟内存映射等功能。项目基于RISC-V架构，支持多核环境下的内存管理，为后续的操作系统功能奠定了坚实的基础。

## 新增功能

### 内存管理模块

1. **物理内存管理 (pmem.c)**
   - 物理页面分配和释放
   - 内核和用户内存区域分离管理
   - 基于链表的空闲页面管理
   - 多核安全的内存分配

2. **虚拟内存管理 (kvm.c)**
   - 页表创建和映射
   - 虚拟地址到物理地址转换
   - 内核页表初始化
   - 内存保护机制

3. **字符串操作 (str.c)**
   - 内存操作函数 (memset, memcpy, memmove)
   - 字符串处理函数 (strlen, strcpy, strcmp)

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

# 清理之前的编译文件
make clean

# 编译内核
make build

# 运行内核 (3核模式)
make qemu
```

### 运行结果
成功运行后应该看到以下输出：
```
cpu 0 is booting!
cpu 2 is booting!
cpu 1 is booting!
mem = 0x00000000821ff000, data = 16843009
mem = 0x00000000821fd000, data = 16843009
mem = 0x00000000821fc000, data = 16843009
...
cpu 0 alloc over
cpu 1 alloc over
cpu 2 alloc over
cpu 0 free over
cpu 1 free over
cpu 2 free over
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
│   ├── mem/                   # 内存管理头文件
│   │   ├── kvm.h              # 虚拟内存管理接口
│   │   ├── pmem.h             # 物理内存管理接口
│   │   └── str.h              # 字符串操作接口
│   ├── lib/
│   │   ├── print.h            # 打印函数声明
│   │   └── lock.h             # 自旋锁接口
│   └── proc/
│       └── cpu.h              # CPU相关定义
├── kernel/                    # 内核源码目录
│   ├── boot/                  # 启动相关代码
│   │   ├── entry.S            # 汇编启动入口
│   │   ├── start.c            # C语言启动代码
│   │   ├── main.c             # 主程序入口 (内存管理测试)
│   │   └── Makefile
│   ├── dev/                   # 设备驱动
│   │   ├── uart.c             # UART串口驱动
│   │   └── Makefile
│   ├── lib/                   # 内核库函数
│   │   ├── print.c            # printf实现
│   │   ├── spinlock.c         # 自旋锁实现
│   │   └── Makefile
│   ├── mem/                   # 内存管理模块
│   │   ├── kvm.c              # 虚拟内存管理实现
│   │   ├── pmem.c             # 物理内存管理实现
│   │   ├── str.c              # 字符串操作实现
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

## 核心功能详解

### 物理内存管理 (pmem.c)

#### 主要功能
- **内存初始化**: `pmem_init()` - 初始化内存管理数据结构
- **页面分配**: `pmem_alloc(bool in_kernel)` - 分配物理页面
- **页面释放**: `pmem_free(void* pa, bool in_kernel)` - 释放物理页面

#### 设计特点
- **分离管理**: 内核和用户内存区域独立管理
- **链表管理**: 使用链表管理空闲页面
- **多核安全**: 使用自旋锁保护临界区
- **动态分配**: 支持按需分配和释放

### 虚拟内存管理 (kvm.c)

#### 主要功能
- **页表操作**: `vm_getpte()` - 获取页表项
- **页面映射**: `vm_mappages()` - 建立虚拟到物理地址映射
- **页面取消映射**: `vm_unmappages()` - 取消页面映射
- **内核页表**: `kvm_init()` - 初始化内核页表

#### 设计特点
- **三级页表**: 支持RISC-V标准的三级页表结构
- **权限控制**: 支持读、写、执行权限控制
- **内存保护**: 实现内存访问保护机制

### 字符串操作 (str.c)

#### 主要功能
- **内存操作**: `memset()`, `memcpy()`, `memmove()`
- **字符串处理**: `strlen()`, `strcpy()`, `strcmp()`

## 测试功能

### 内存分配测试
项目实现了完整的内存管理测试，包括：

1. **多核并行分配**: 3个CPU核心同时进行内存分配
2. **内存初始化**: 使用memset将分配的内存初始化为特定值
3. **数据验证**: 验证内存分配和初始化的正确性
4. **内存释放**: 正确释放所有分配的内存

### 测试流程
```
CPU 0: 分配512个页面 → 初始化 → 等待其他CPU → 释放内存
CPU 1: 分配512个页面 → 初始化 → 等待其他CPU → 释放内存  
CPU 2: 分配512个页面 → 初始化 → 等待其他CPU → 释放内存
```

## 技术特点

- **RISC-V架构**: 支持M模式和S模式切换
- **多核支持**: 基于CLINT的核间通信
- **内存管理**: 完整的物理和虚拟内存管理
- **同步机制**: 自旋锁保护临界区
- **模块化设计**: 清晰的模块分离和接口设计
