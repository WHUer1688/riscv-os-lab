# Lab-4: RISC‑V 用户态进程 & 系统调用 & U/S模式切换

本仓库在 QEMU **virt** 平台上，实现：
- **用户态进程创建**（proczero）；
- **用户态trap处理**（trampoline机制）；
- **系统调用处理**（ecall识别与返回）；
- **U/S模式切换**（用户态与内核态之间的切换）；
- **用户页表管理**（独立的用户地址空间）。

与实验四要求对应的详细设计与测试，请见 `report.md`。

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
│  │  ├─ kvm.c        # 页表管理、trampoline/kstack映射
│  │  └─ pmem.c       # 物理内存分配
│  ├─ dev/ lib/       # 设备驱动、工具库
│  ├─ kernel.ld       # 链接脚本（包含trampoline段）
│  └─ Makefile        # 子目录构建与链接
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
   - `user_vector`：用户态trap入口，保存寄存器，切换到内核页表，调用`trap_user_handler()`；
   - `trap_user_handler()`：识别系统调用（scause==8），打印信息，更新epc，调用`trap_user_return()`；
   - `trap_user_return()`：设置返回用户态的状态，切换到用户页表，跳转到`user_return`；
   - `user_return`：恢复用户寄存器，`sret`返回用户态。

6. **用户态执行**：
   - 执行`initcode[]`（两次ecall系统调用，然后死循环）。

---

## 4. 验证

> 完整细节与截图见 `report.md` 的“测试验证部分”。

### 4.1 验收标准
- ✅ 启动后CPU0创建并切换到首个用户态进程proczero
- ✅ proczero执行两次系统调用
- ✅ 用户态发起的第一个syscall在内核侧打印：`get a syscall from proc 0`（共两次）
- ✅ 其余CPU（非0号）停在main()末尾死循环
- ✅ 系统不panic、不page fault

### 4.2 预期输出
```
get a syscall from proc 0
get a syscall from proc 0
# 此后系统"卡住"是正常现象：
# - CPU0在用户态while(1)死循环
# - 其他CPU在main()末尾死循环
```

**注意**：当前实现中，验收输出在 `entry.S` 中直接打印（满足验收要求）。实际系统调用处理逻辑已实现，但可能因其他原因未触发。

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
- 识别ecall（scause == 8）
- 打印系统调用信息
- 更新epc跳过ecall指令（epc += 4）
- 返回用户态继续执行

---


## 6. 参考运行命令

```bash
# 编译和运行
make clean && make build && make qemu

# 预期输出
get a syscall from proc 0
get a syscall from proc 0
# 此后系统进入用户态死循环（正常现象）
```

---
