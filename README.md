# Lab-5: 系统调用

本仓库在 QEMU **virt** 平台上，实现：
- **用户态进程创建**（proczero）；
- **用户态trap处理**（trampoline机制）；
- **系统调用处理**（ecall识别、分发、参数提取、返回值处理）；
- **U/S模式切换**（用户态与内核态之间的切换）；
- **用户页表管理**（独立的用户地址空间）；
- **用户内存安全访问**（copyin/copyout/fetchstr）。

与实验五要求对应的详细设计与测试，请见 `report.md`。

---

## 1. 快速开始（Quick Start）

### 1.1 依赖
- `qemu-system-riscv64`
- `riscv64-linux-gnu-gcc` / `riscv64-linux-gnu-ld`（或等价交叉工具链）
- GNU make

### 1.2 构建与运行
```bash
# 编译
make clean && make build

# 运行
make qemu
```

> 运行后 QEMU 使用 `-nographic`，**当前终端就是串口**。

### 1.3 QEMU 控件
- `Ctrl-A c`：在 **串口 ↔ QEMU monitor** 间切换；
- `Ctrl-A x`：退出 QEMU；
- `Ctrl-A h`：查看帮助。

---

## 2. 目录结构（关键文件）

```
whu-oslab-lab1/
├─ include/
│  ├─ common.h / memlayout.h / riscv.h
│  ├─ proc/
│  │  ├─ proc.h      # proc_t, trapframe_t, context_t 定义
│  │  └─ cpu.h       # cpu_t 定义
│  ├─ syscall.h      # 系统调用号定义
│  └─ trap.h         # 内核trap相关
├─ kernel/
│  ├─ boot/
│  │  ├─ entry.S      # M 态早期引导、.bss初始化
│  │  ├─ start.c      # PMP、委托、mtvec=timer_vector、mret→S
│  │  └─ main.c       # 初始化、proc_make_first()创建proczero
│  ├─ proc/
│  │  ├─ proc.c       # proc_pgtbl_init(), proc_make_first()
│  │  └─ swtch.S      # 上下文切换
│  ├─ trap/
│  │  ├─ trap.S       # S 态内核trap入口 kernel_vector
│  │  ├─ trap_kernel.c# 内核trap处理
│  │  ├─ trampoline.S # 用户态trap入口 user_vector, user_return
│  │  └─ trap_user.c  # 用户态trap处理 trap_user_handler
│  ├─ mem/
│  │  ├─ kvm.c        # 页表管理、trampoline/kstack映射、用户内存访问
│  │  └─ pmem.c       # 物理内存分配
│  ├─ syscall/
│  │  ├─ syscall.c    # 系统调用分发器、参数提取
│  │  ├─ sysproc.c    # 进程相关系统调用（getpid/fork/exit/wait/kill/sbrk）
│  │  └─ sysfile.c    # 文件相关系统调用（open/close/read/write）
│  ├─ dev/ lib/       # 设备驱动、工具库
│  ├─ kernel.ld       # 链接脚本（包含trampoline段）
│  └─ Makefile        # 子目录构建与链接
├─ user/
│  ├─ user.h           # 用户态系统调用函数声明
│  ├─ syscall.h        # 用户态系统调用号定义
│  ├─ usys.pl          # 生成用户态系统调用桩代码
│  └─ test_syscall.c   # 系统调用测试程序
├─ Makefile            # 顶层构建与运行（qemu）
├─ report.md           # 实验四综合报告
└─ README.md           # 本文件
```

---

## 3. 工作流程

1. **entry.S（M 态）**：
   - 设置 per‑hart 栈，`tp=mhartid`；
  

2. **start.c（M 态）**：
   - **PMP**：`pmpaddr0=~0, pmpcfg0=0x0F` 放行物理地址；
   - **委托**：`mideleg` 委托 **SSIP/SEIP** 到 S 态；
   - **mtvec**：`mtvec=timer_vector`，开启 `MIE.MTIE`；
   - `mepc=main, mstatus.MPP=S, mret` 进入 S 态（hart0 唤醒其他核）。

3. **main.c（S 态）**：
   - CPU0：`pmem_init()` → `kvm_init()` → `trap_kernel_init()`；
   - 所有CPU：`kvm_inithart()` → `trap_kernel_inithart()`；
   - 其他CPU：死循环；
   - CPU0：`proc_make_first()` 创建proczero并切换到用户态。

4. **proc.c**：
   - `proc_pgtbl_init()`：建立用户页表（代码、栈、trapframe、trampoline映射）；
   - `proc_make_first()`：创建proczero，设置trapframe和context，`swtch()`切换到proczero。

5. **trap_user.c / trampoline.S**：
   - `user_vector`：用户态trap入口，保存寄存器到trapframe，切换到内核页表，调用`trap_user_handler()`；
   - `trap_user_handler()`：识别系统调用（scause==8），跳过ecall指令（epc += 4），开启中断，调用`syscall()`；
   - `syscall()`：从trapframe->a7获取系统调用号，调用对应的系统调用函数，将返回值写入trapframe->a0；
   - `trap_user_return()`：设置返回用户态的状态，切换到用户页表，跳转到`user_return`；
   - `user_return`：恢复用户寄存器，`sret`返回用户态。

6. **syscall.c / sysproc.c / sysfile.c**：
   - `syscall()`：系统调用分发器，根据系统调用号调用对应函数；
   - `argint()/argaddr()/argstr()`：从trapframe安全提取用户态参数；
   - `sys_getpid()`：已实现，返回当前进程ID；
   - 其他系统调用：框架已搭建，待实现具体功能。

7. **用户态执行**：
   - 执行`initcode[]`（两次ecall系统调用，然后死循环）。

---

## 4. 验证

> 完整细节与截图见 `report.md` 的“测试验证部分”。

### 4.1 验收标准
- ✅ 启动后CPU0创建并切换到首个用户态进程proczero
- ✅ proczero执行两次系统调用
- ✅ 用户态发起的系统调用被正确识别和处理
- ✅ 系统调用全链路打通：用户态 → ecall → trap → syscall() → 返回用户态
- ✅ 其余CPU（非0号）停在main()末尾死循环
- ✅ 系统不panic、不page fault

### 4.2 预期输出
```
# 系统启动和初始化信息...
# 系统调用处理（如果initcode中的系统调用被触发）
# 此后系统"卡住"是正常现象：
# - CPU0在用户态while(1)死循环
# - 其他CPU在main()末尾死循环
```

### 4.3 系统调用测试
系统调用全链路已实现，包括：
- ✅ ecall识别和trap处理
- ✅ 系统调用分发器
- ✅ 参数提取函数（argint/argaddr/argstr）
- ✅ 用户内存安全访问（copyin/copyout/fetchstr）
- ✅ getpid系统调用（最小闭环）
- ✅ 用户态接口文件（user.h, usys.pl）

可以通过修改`initcode[]`中的系统调用号来测试不同的系统调用。

---

## 5. 关键实现

### 5.1 用户页表初始化
- `proc_pgtbl_init()`：建立用户地址空间
  - trampoline页（TRAMPOLINE，可执行）
  - trapframe页（TRAMPOLINE - PGSIZE，可读写）
  - 用户代码段（VA 0，包含initcode，可执行）
  - 用户栈（0x80000000 - PGSIZE，可读写）

### 5.2 Trampoline机制
- trampoline页在内核页表和用户页表中都映射到相同虚拟地址（TRAMPOLINE）
- `user_vector`：用户态trap入口，必须使用用户页表中的TRAMPOLINE地址设置`stvec`
- `user_return`：从内核返回用户态，恢复寄存器并`sret`

### 5.3 系统调用处理
- **ecall识别**：在`trap_user_handler()`中识别`scause == 8`（用户态ecall）
- **指令跳过**：更新epc跳过ecall指令（`epc += 4`），避免无限循环
- **中断开启**：调用`intr_on()`允许中断（xv6标准做法）
- **系统调用分发**：`syscall()`从`trapframe->a7`获取系统调用号，调用对应函数
- **参数提取**：
  - `argint(n, &ip)`：提取第n个整数参数
  - `argaddr(n, &ip)`：提取第n个地址参数
  - `argstr(n, buf, max)`：提取第n个字符串参数（使用`fetchstr`安全访问用户内存）
- **返回值处理**：系统调用返回值写入`trapframe->a0`，返回用户态后自动获取

### 5.4 用户内存安全访问
- **copyin(pgtbl, dst, srcva, len)**：从用户空间复制数据到内核空间
- **copyout(pgtbl, dstva, src, len)**：从内核空间复制数据到用户空间
- **fetchstr(pgtbl, addr, buf, max)**：从用户空间安全读取字符串
- 所有函数都通过页表检查确保地址有效且权限正确，避免内核崩溃

### 5.5 已实现的系统调用
- ✅ **getpid**：获取当前进程ID（最小闭环已实现）
- 🔄 **fork/exit/wait/kill/sbrk**：框架已搭建，待实现具体功能
- 🔄 **open/close/read/write**：框架已搭建，待实现具体功能

---


## 6. 系统调用全链路流程

```
用户态函数调用
    ↓
用户态桩代码（usys.S）
    ├─ li a7, SYS_getpid  # 设置系统调用号
    ├─ ecall              # 陷入内核
    └─ ret                # 返回（返回值在a0中）
    ↓
硬件trap处理
    ├─ 跳转到 user_vector（trampoline.S）
    ├─ 保存用户寄存器到 trapframe
    ├─ 切换到内核页表和内核栈
    └─ 调用 trap_user_handler()
    ↓
trap_user_handler()（trap_user.c）
    ├─ 识别 scause == 8（用户态ecall）
    ├─ tf->epc += 4（跳过ecall指令）
    ├─ intr_on()（开启中断）
    └─ 调用 syscall()
    ↓
syscall()（syscall.c）
    ├─ 从 trapframe->a7 获取系统调用号
    ├─ 调用 syscalls[num]()（如 sys_getpid）
    └─ 将返回值写入 trapframe->a0
    ↓
sys_getpid()（sysproc.c）
    └─ 返回 myproc()->pid
    ↓
trap_user_return()（trap_user.c）
    ├─ 设置返回用户态的状态
    ├─ 切换到用户页表
    └─ 跳转到 user_return
    ↓
user_return（trampoline.S）
    ├─ 恢复用户寄存器
    └─ sret（返回用户态）
    ↓
用户态继续执行
    └─ 返回值在 a0 寄存器中
```

## 7. 参考运行命令

```bash
# 编译和运行
make clean && make build && make qemu

# 预期行为
# - 系统启动并创建proczero进程
# - proczero执行initcode中的系统调用
# - 系统调用被正确处理并返回用户态
# - 最终进入用户态死循环（正常现象）
```

---
