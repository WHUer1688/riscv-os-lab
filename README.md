# Lab-3: RISC‑V 定时器中断 & PLIC(UART) & 多核启动

本仓库在 QEMU **virt** 平台上，按照**正常方法**实现：
- M 态引导 → 委托 S 态处理陷阱；
- **M 态计时器中断** 触发，**S 态软件中断** 应答（SSIP）；
- **PLIC 外部中断** 路由 UART0（接收回显）；
- **多核启动** 与 per‑hart 初始化；
- S 态统一陷阱入口与分派。

与实验三要求对应的详细设计与测试，请见 `report.md`。

---

## 1. 快速开始（Quick Start）

### 1.1 依赖
- `qemu-system-riscv64`
- `riscv64-linux-gnu-gcc` / `riscv64-linux-gnu-ld`（或等价交叉工具链）
- GNU make

### 1.2 构建与运行
```bash
# 10 Hz 滴答（INTERVAL=1,000,000 对应 QEMU virt 的 10MHz timebase）
make clean && make -B kernel INTERVAL=1000000 && make qemu

# 50 Hz 滴答
make clean && make -B kernel INTERVAL=200000 && make qemu
```

> 运行后 QEMU 使用 `-nographic`，**当前终端就是串口**：直接在此终端键入字符，可被内核回显。

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
│  └─ dev/
│     ├─ uart.h
│     ├─ plic.h
│     └─ timer.h
├─ kernel/
│  ├─ boot/
│  │  ├─ entry.S      # M 态早期引导、设置栈、保存 mhartid
│  │  ├─ start.c      # PMP、委托、mtvec=timer_vector、mret→S
│  │  └─ main.c       # stvec、PLIC init/inithart、UART、MMU、timer_init、intr_on
│  ├─ dev/
│  │  ├─ uart.c       # 16550 初始化与中断回显
│  │  ├─ plic.c       # PLIC init/inithart、claim/complete
│  │  └─ timer.c      # 与 CLINT 交互，重装 mtimecmp
│  ├─ trap/
│  │  ├─ trap.S       # S 态统一陷阱入口 kernel_vector
│  │  └─ trap_kernel.c# scause 分派：SSIP/SEIP/异常
│  ├─ mem/ lib/ proc/ # 其他子系统
│  ├─ kernel.ld       # 链接脚本
│  └─ Makefile        # 子目录构建与链接
├─ Makefile            # 顶层构建与运行（qemu）
├─ report.md           # 实验三综合报告（正常方法）
└─ README.md           # 本文件
```

---

## 3. 工作流程

1. **entry.S（M 态）**：设置 per‑hart 栈，`tp=mhartid`，跳转 `start()`；  
2. **start.c（M 态）**：
   - **PMP**：教学场景采用 `pmpaddr0=~0, pmpcfg0=0x0F` 放行物理地址；
   - **委托**：`mideleg` 委托 **SSIP/SEIP** 到 S 态；
   - **mtvec**：`mtvec=timer_vector`，开启 `MIE.MTIE`；
   - `mepc=main, mstatus.MPP=S, mret` 进入 S 态（hart0 唤醒其他核）。
3. **main.c（S 态）**：
   - `stvec=kernel_vector`；
   - `plic_init()` + `plic_inithart()`；`uart_init()`；
   - `pmem/kvm` 与 `satp`；
   - `timer_init()` 初次装载 `mtimecmp`；
   - `intr_on()`；hart0 设置 `started=1`，其他核过屏障后各自 `inithart()`。  
4. **trap.S / trap_kernel.c**：
   - `kernel_vector` 保存现场 → `trap_kernel_handler()`；
   - **SSIP** → `timer_on_tick()`（`ticks++`、重装 `mtimecmp`、清 `SIP.SSIP`）；
   - **SEIP** → `plic_claim()`，`irq==UART0` 时 `uart_intr()` 回显，`plic_complete(irq)`。
5. **dev/ 子系统**：UART/PLIC/Timer 的 MMIO 驱动实现。

---

## 4. 验证

> 完整细节与截图见 `report.md` 的“测试验证部分”。

- **多核启动**：看到 `>>>`（每 hart 一次）与 boot 日志；
- **定时器滴答**：周期性 `T` 与 `ticks=...` 打印；
- **UART 回显**：在终端敲击字符可被回显（中断链路闭环）；
- **异常路径**（可选）：触发非法指令或页故障，`trap_kernel_handler` 打印 `scause/stval/sepc`。

**示例输出（节选）**
```
>>>               # 3 个 hart 的早期冒烟
[BOOT] cpu 0
[TRAP/PLIC] ready
[MMU] satp enabled
[TIMER] init ok
[INT] SIE=1, running...
T
ticks=1
T
ticks=2
...
```

---

## 5. 配置项

- `INTERVAL`：滴答间隔（单位：CLINT timebase tick，QEMU virt 缺省 10MHz）。
  - `make ... INTERVAL=1000000` → **10 Hz**
  - `make ... INTERVAL=200000`  → **50 Hz**
- 可在 `Makefile` 传递：`CFLAGS+=-DINTERVAL=$(INTERVAL)`。

---

## 6. 故障排查

- **只看到 `>>>`/`SSS` 没有后续**  
  检查顺序：`PMP` → `stvec` → `plic_inithart()` → `intr_on()` → `timer_init()` 是否装载首个 `mtimecmp`。

- **输入不回显**  
  可能在 QEMU monitor：按 `Ctrl-A c` 切回串口；确认 `uart_init()` 打开接收中断、`plic_inithart()` 使能 UART IRQ。

- **S 态异常（取指/访存）**  
  确认 **PMP** 已在 `mret` 前放行；核对 `satp` 生效后的页表映射。

- **链接缺对象**  
  确保 `trap/ dev/` 的 `.o` 在 `kernel/Makefile` 的最终链接目标中。

---

## 7. 与 xv6 的关系

实现路径与 xv6‑riscv 的思路一致（M 态定时器触发 + S 态软件中断应答、PLIC 外设中断、S 态统一陷阱），但本实验在教学场景下：
- 使用 **PMP 全开** 简化前期调试；
- 强化了 **初始化顺序**（先 `stvec` 再外设/内存初始化），降低早期异常风险。

---
