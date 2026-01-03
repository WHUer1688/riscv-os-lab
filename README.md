# Lab-6: 进程管理与调度

本仓库在 QEMU **virt** 平台上，实现：
- **用户态进程创建**（proczero）；
- **用户态trap处理**（trampoline机制）；
- **系统调用处理**（ecall识别、分发、参数提取、返回值处理）；
- **U/S模式切换**（用户态与内核态之间的切换）；
- **用户页表管理**（独立的用户地址空间）；
- **用户内存安全访问**（copyin/copyout/fetchstr）；
- **进程管理**（进程数组、进程分配与释放、fork/exit/wait）；
- **进程调度**（RR时间片轮转调度、时间片递减与抢占）；
- **sleep/wakeup机制**（进程同步与等待）。

与实验六要求对应的详细设计与测试，请见 `report.md`。

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
│  │  └─ main.c       # 初始化、proc_init()、proc_make_first()
│  ├─ proc/
│  │  ├─ proc.c       # 进程管理（init/alloc/free/fork/exit/wait/scheduler）
│  │  └─ swtch.S      # 上下文切换
│  ├─ trap/
│  │  ├─ trap.S       # S 态内核trap入口 kernel_vector
│  │  ├─ trap_kernel.c# 内核trap处理（时间片递减）
│  │  ├─ trampoline.S # 用户态trap入口 user_vector, user_return
│  │  └─ trap_user.c  # 用户态trap处理（时间片递减）
│  ├─ mem/
│  │  ├─ kvm.c        # 页表管理、trampoline/kstack映射、用户内存访问
│  │  └─ pmem.c       # 物理内存分配
│  ├─ syscall/
│  │  ├─ syscall.c    # 系统调用分发器、参数提取
│  │  ├─ sysproc.c    # 进程相关系统调用（fork/exit/wait/sleep/print/brk/mmap）
│  │  └─ sysfile.c    # 文件相关系统调用（open/close/read/write）
│  ├─ dev/
│  │  └─ timer.c      # 时钟中断处理（ticks、wakeup）
│  ├─ lib/            # 工具库（print、spinlock）
│  ├─ kernel.ld       # 链接脚本（包含trampoline段）
│  └─ Makefile        # 子目录构建与链接
├─ user/
│  ├─ user.h           # 用户态系统调用函数声明
│  ├─ syscall.h        # 用户态系统调用号定义
│  ├─ sys.h            # 通用系统调用包装函数
│  ├─ usys.pl          # 生成用户态系统调用桩代码
│  ├─ test.c           # 进程管理测试程序
│  └─ test_syscall.c   # 系统调用测试程序
├─ Makefile            # 顶层构建与运行（qemu）
├─ report.md           # 实验报告
└─ README.md           # 本文件
```

---

## 3. 工作流程

1. **entry.S（M 态）**：
   - 设置 per‑hart 栈，`tp=mhartid`；
   - 初始化 `.bss` 段（仅CPU0执行）；

2. **start.c（M 态）**：
   - **PMP**：`pmpaddr0=~0, pmpcfg0=0x0F` 放行物理地址；
   - **委托**：`mideleg` 委托 **SSIP/SEIP** 到 S 态；
   - **mtvec**：`mtvec=timer_vector`，开启 `MIE.MTIE`；
   - `mepc=main, mstatus.MPP=S, mret` 进入 S 态（hart0 唤醒其他核）。

3. **main.c（S 态）**：
   - CPU0：`pmem_init()` → `kvm_init()` → `trap_kernel_init()` → `proc_init()`；
   - 所有CPU：`kvm_inithart()` → `trap_kernel_inithart()`；
   - 其他CPU：死循环（或运行调度器）；
   - CPU0：`proc_make_first()` 创建proczero并切换到用户态。

4. **proc.c**：
   - `proc_init()`：初始化进程数组、锁、创建proczero；
   - `proc_alloc()`：从进程数组分配新进程，分配PID、页表、trapframe；
   - `proc_free()`：释放进程资源（页表、trapframe）；
   - `proc_make_first()`：使用`proc_alloc()`创建proczero，直接调用`fork_return()`进入用户态；
   - `proc_fork()`：复制进程（复制页表、trapframe、设置父子关系）；
   - `proc_exit()`：进程退出（设置ZOMBIE状态、reparent、唤醒父进程）；
   - `proc_wait()`：等待子进程退出（使用sleep/wakeup机制）；
   - `proc_scheduler()`：RR调度器主循环，选择RUNNABLE进程运行；
   - `proc_sched()`：切换到另一个进程（保存/恢复上下文）；
   - `proc_yield()`：让出CPU（时间片用尽或主动让出）；
   - `proc_sleep()` / `proc_wakeup()`：进程睡眠与唤醒机制。

5. **trap_user.c / trap_kernel.c**：
   - **时钟中断处理**：在`trap_user_handler()`和`trap_kernel_handler()`中处理时钟中断；
   - **时间片递减**：RUNNING进程的时间片减1，为0时调用`proc_yield()`触发调度；
   - **系统调用处理**：识别ecall，调用`syscall()`分发系统调用。

6. **syscall.c / sysproc.c**：
   - `syscall()`：系统调用分发器，根据系统调用号调用对应函数；
   - `argint()/argaddr()/argstr()`：从trapframe安全提取用户态参数；
   - `sys_fork()`：调用`proc_fork()`创建子进程；
   - `sys_exit()`：调用`proc_exit()`退出进程；
   - `sys_wait()`：调用`proc_wait()`等待子进程；
   - `sys_sleep()`：使用ticks和sleep/wakeup实现睡眠；
   - `sys_print()`：打印用户空间字符串；
   - `sys_brk()`：堆管理（获取/设置堆顶）；
   - `sys_mmap()`：内存映射（简化实现）。

7. **timer.c**：
   - `timer_on_tick()`：每次时钟中断时递增ticks，调用`proc_wakeup()`唤醒sleep的进程。

8. **用户态执行**：
   - 执行`initcode[]`或加载的用户程序；
   - 通过系统调用进行进程操作、内存管理等。

---

## 4. 验证

> 完整细节与截图见 `report.md` 的"测试验证部分"。

### 4.1 验收标准
- ✅ 启动后CPU0创建并切换到首个用户态进程proczero
- ✅ 进程管理三件套（proc_init/proc_alloc/proc_free）正常工作
- ✅ fork系统调用能正确创建子进程
- ✅ exit/wait系统调用能正确处理进程退出和等待
- ✅ RR调度器能正确切换进程
- ✅ 时间片递减和抢占机制正常工作
- ✅ sleep/wakeup机制正常工作
- ✅ 系统调用全链路打通：用户态 → ecall → trap → syscall() → 返回用户态
- ✅ 系统不panic、不page fault

### 4.2 预期输出
```
# 系统启动和初始化信息...
# 时钟中断输出（T字符和ticks计数）
# 进程操作输出（fork/exit/wait）
# 调度器切换进程
# 系统调用处理输出
```

### 4.3 系统调用测试
已实现的系统调用包括：
- ✅ **fork**：创建子进程
- ✅ **exit**：退出进程
- ✅ **wait**：等待子进程退出
- ✅ **sleep**：睡眠指定时间
- ✅ **print**：打印字符串
- ✅ **getpid**：获取进程ID
- ✅ **brk**：堆管理
- ✅ **mmap**：内存映射（简化实现）
- 🔄 **sbrk**：框架已搭建，待完善
- 🔄 **open/close/read/write**：框架已搭建，待实现

可以通过修改`initcode[]`或加载用户程序来测试不同的系统调用。

---

## 5. 关键实现

### 5.1 进程管理

#### 5.1.1 进程结构体
```c
typedef struct proc {
    enum proc_state state;    // 进程状态（UNUSED/USED/SLEEPING/RUNNABLE/RUNNING/ZOMBIE）
    int pid;                  // 进程ID
    struct proc *parent;      // 父进程指针
    int exit_state;           // 退出状态
    void *sleep_space;        // sleep的channel
    pgtbl_t pgtbl;            // 用户态页表
    trapframe_t* tf;          // trapframe
    uint64 kstack;            // 内核栈虚拟地址
    context_t ctx;            // 内核上下文
    int time_slice;           // 时间片计数
    spinlock_t lk;            // 进程锁
    // ...
} proc_t;
```

#### 5.1.2 进程初始化（proc_init）
- 初始化全局进程数组`procs[NPROC]`
- 初始化每个进程的锁和kstack地址
- 创建proczero（pid=0）

#### 5.1.3 进程分配（proc_alloc）
- 从进程数组找UNUSED进程
- 分配PID（使用全局pid锁保护）
- 分配trapframe和用户页表
- 初始化context（ra指向fork_return）

#### 5.1.4 进程释放（proc_free）
- 释放用户页表和用户内存
- 释放trapframe
- 清空进程字段，设置state=UNUSED

### 5.2 进程操作

#### 5.2.1 Fork（proc_fork）
1. 调用`proc_alloc()`分配新进程
2. 复制用户内存（页表和物理页）
3. 复制trapframe（子进程返回值a0=0）
4. 设置父子关系
5. 设置子进程状态为RUNNABLE

#### 5.2.2 Exit（proc_exit）
1. 处理"父死子活"问题（reparent到proczero）
2. 设置退出状态和ZOMBIE状态
3. 唤醒父进程（`proc_wakeup_one(parent)`）
4. 调用`proc_sched()`让出CPU

#### 5.2.3 Wait（proc_wait）
1. 扫描所有进程，找子进程
2. 如果找到ZOMBIE子进程：复制退出状态，回收进程，返回
3. 如果有子进程但都没退出：调用`proc_sleep()`等待
4. 如果没有子进程：返回-1

### 5.3 进程调度

#### 5.3.1 RR调度器（proc_scheduler）
- 调度器主循环：遍历进程数组，找RUNNABLE进程
- 选中进程后：设置state=RUNNING，切换到该进程
- 被切回后：重新获取进程锁，继续循环

#### 5.3.2 调度函数（proc_sched）
- 前置检查：中断必须关闭，当前进程必须正确
- 保存中断状态，释放进程锁
- 调用`swtch()`切换到调度器上下文
- 被切回后恢复中断状态

#### 5.3.3 Yield（proc_yield）
- 设置进程状态为RUNNABLE
- 重置时间片为DEFAULT_SLICE
- 调用`proc_sched()`让出CPU

### 5.4 时间片与抢占

#### 5.4.1 时间片字段
- 每个进程有`time_slice`字段
- 进程变为RUNNABLE时重置为DEFAULT_SLICE（10）

#### 5.4.2 时钟中断处理
- 在`trap_user_handler()`和`trap_kernel_handler()`中处理时钟中断
- 如果当前进程是RUNNING：时间片减1
- 如果时间片为0：调用`proc_yield()`触发调度并重置时间片

### 5.5 Sleep/Wakeup机制

#### 5.5.1 Sleep（proc_sleep）
1. 获取进程锁
2. 释放外部锁
3. 设置sleep_space和SLEEPING状态
4. 调用`proc_sched()`让出CPU
5. 被唤醒后：清sleep_space，释放进程锁，重新获取外部锁

#### 5.5.2 Wakeup（proc_wakeup）
- 遍历所有进程，唤醒所有`state==SLEEPING && sleep_space==chan`的进程
- 设置状态为RUNNABLE，重置时间片

#### 5.5.3 Wakeup_one（proc_wakeup_one）
- 只唤醒指定进程（用于exit时唤醒父进程）

### 5.6 系统调用实现

#### 5.6.1 sys_fork
- 调用`proc_fork()`创建子进程
- 返回子进程PID（父进程）或0（子进程）

#### 5.6.2 sys_exit
- 调用`proc_exit()`退出进程（不会返回）

#### 5.6.3 sys_wait
- 调用`proc_wait()`等待子进程退出
- 返回子进程PID或-1

#### 5.6.4 sys_sleep
- 计算deadline = ticks + n
- 循环检查ticks，如果未到deadline则sleep
- 使用`proc_sleep(&ticks_lock, &ticks_lock)`

#### 5.6.5 sys_print
- 从用户空间读取字符串（使用`fetchstr`）
- 调用`printf()`打印

#### 5.6.6 sys_brk
- 如果addr==0：返回当前堆顶
- 否则：设置新的堆顶

#### 5.6.7 sys_mmap
- 简化实现：直接返回地址（实际应该分配内存并映射）

### 5.7 用户页表初始化
- `proc_pgtbl_init()`：建立用户地址空间
  - trampoline页（TRAMPOLINE，可执行）
  - trapframe页（TRAMPOLINE - PGSIZE，可读写）
  - 用户代码段（VA 0，包含initcode，可执行）
  - 用户栈（0x80000000 - PGSIZE，可读写）

### 5.8 Trampoline机制
- trampoline页在内核页表和用户页表中都映射到相同虚拟地址（TRAMPOLINE）
- `user_vector`：用户态trap入口，必须使用用户页表中的TRAMPOLINE地址设置`stvec`
- `user_return`：从内核返回用户态，恢复寄存器并`sret`

### 5.9 用户内存安全访问
- **copyin(pgtbl, dst, srcva, len)**：从用户空间复制数据到内核空间
- **copyout(pgtbl, dstva, src, len)**：从内核空间复制数据到用户空间
- **fetchstr(pgtbl, addr, buf, max)**：从用户空间安全读取字符串
- 所有函数都通过页表检查确保地址有效且权限正确，避免内核崩溃

---

## 6. 系统调用全链路流程

```
用户态函数调用
    ↓
用户态桩代码（usys.S）或syscall()包装函数
    ├─ li a7, SYS_fork  # 设置系统调用号
    ├─ ecall            # 陷入内核
    └─ ret              # 返回（返回值在a0中）
    ↓
硬件trap处理
    ├─ 跳转到 user_vector（trampoline.S）
    ├─ 保存用户寄存器到 trapframe
    ├─ 切换到内核页表和内核栈
    └─ 调用 trap_user_handler()
    ↓
trap_user_handler()（trap_user.c）
    ├─ 处理时钟中断（时间片递减）
    ├─ 识别 scause == 8（用户态ecall）
    ├─ tf->epc += 4（跳过ecall指令）
    ├─ intr_on()（开启中断）
    └─ 调用 syscall()
    ↓
syscall()（syscall.c）
    ├─ 从 trapframe->a7 获取系统调用号
    ├─ 调用 syscalls[num]()（如 sys_fork）
    └─ 将返回值写入 trapframe->a0
    ↓
sys_fork()（sysproc.c）
    └─ 调用 proc_fork() 创建子进程
    ↓
proc_fork()（proc.c）
    ├─ proc_alloc() 分配新进程
    ├─ 复制用户内存
    ├─ 复制trapframe
    └─ 返回子进程PID
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

## 7. 进程调度流程

```
进程A运行中（RUNNING）
    ↓
时钟中断（时间片减1）
    ↓
时间片为0？
    ├─ 是 → proc_yield()
    │      ├─ state = RUNNABLE
    │      ├─ time_slice = DEFAULT_SLICE
    │      └─ proc_sched()
    │          └─ swtch(&p->ctx, &cpu->ctx)
    │              ↓
    └─ 否 → 继续运行
            ↓
调度器（proc_scheduler）
    ├─ 遍历进程数组
    ├─ 找到RUNNABLE进程B
    ├─ state = RUNNING
    └─ swtch(&cpu->ctx, &p->ctx)
        ↓
进程B运行中（RUNNING）
```

## 8. Sleep/Wakeup流程

```
进程A调用sys_sleep(n)
    ↓
sys_sleep()
    ├─ deadline = ticks + n
    └─ while(ticks < deadline)
        └─ proc_sleep(&ticks_lock, &ticks_lock)
            ├─ state = SLEEPING
            ├─ sleep_space = &ticks_lock
            └─ proc_sched() 让出CPU
                ↓
调度器选择其他进程运行
    ↓
时钟中断（timer_on_tick）
    ├─ ticks++
    └─ proc_wakeup(&ticks_lock)
        └─ 唤醒所有在ticks_lock上sleep的进程
            ↓
进程A被唤醒
    ├─ state = RUNNABLE
    └─ 继续检查ticks < deadline
```

## 9. 参考运行命令

```bash
# 编译和运行
make clean && make build && make qemu

# 预期行为
# - 系统启动并初始化进程系统
# - 创建proczero进程并切换到用户态
# - 执行initcode或用户程序
# - 系统调用被正确处理
# - 进程调度和时间片管理正常工作
# - 时钟中断定期触发并输出
```

---

## 10. 实验六新增功能总结

### 10.1 进程管理
- ✅ 全局进程数组`procs[NPROC]`
- ✅ 进程分配与释放（proc_alloc/proc_free）
- ✅ 进程初始化（proc_init）
- ✅ 进程状态管理（UNUSED/USED/SLEEPING/RUNNABLE/RUNNING/ZOMBIE）

### 10.2 进程操作
- ✅ Fork：进程复制
- ✅ Exit：进程退出（ZOMBIE状态、reparent）
- ✅ Wait：等待子进程退出（使用sleep/wakeup）

### 10.3 进程调度
- ✅ RR时间片轮转调度
- ✅ 时间片递减与抢占
- ✅ 调度器主循环（proc_scheduler）
- ✅ 进程切换（proc_sched/proc_yield）

### 10.4 同步机制
- ✅ Sleep/Wakeup机制
- ✅ 进程锁保护

### 10.5 系统调用
- ✅ sys_fork
- ✅ sys_exit
- ✅ sys_wait
- ✅ sys_sleep
- ✅ sys_print
- ✅ sys_brk
- ✅ sys_mmap（简化实现）

---
