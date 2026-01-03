# Lab-7: 文件系统

本仓库在 QEMU **virt** 平台上，实现：
- **VirtIO 磁盘驱动**（MMIO 映射、中断处理、块读写）；
- **Buf Cache**（LRU 缓存策略、懒惰写回）；
- **文件系统初始化**（super block 读取、inode 缓存初始化）；
- **Bitmap 管理**（数据块和 inode 的分配与回收）；
- **Inode 层**（数据索引、跨块读写、10+2*N+N*N 索引结构）；
- **目录与路径解析**（目录项管理、路径查找）；
- **文件系统调用**（open/read/write/close、文件描述符管理）；
- **ELF 文件加载**（proc_exec、从文件系统加载可执行文件）。

与实验七要求对应的详细设计与测试，请见 `report.md`。

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

# 运行（会自动创建 fs.img）
make qemu
```

> 运行后 QEMU 使用 `-nographic`，**当前终端就是串口**。
> 
> 注意：首次运行会自动创建空的 `fs.img` 文件（10MB）。如需使用有效的文件系统，需要先格式化文件系统映像。

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
│  ├─ trap.h         # 内核trap相关
│  ├─ dev/
│  │  ├─ virtio.h    # VirtIO 磁盘驱动接口
│  │  └─ plic.h      # PLIC 中断控制器
│  └─ fs/
│     ├─ buf.h        # Buf Cache 接口
│     ├─ fs.h         # 文件系统接口（super block）
│     ├─ bitmap.h     # Bitmap 管理接口
│     ├─ inode.h      # Inode 接口
│     ├─ dir.h        # 目录操作接口
│     └─ file.h       # 文件结构接口
├─ kernel/
│  ├─ boot/
│  │  ├─ entry.S      # M 态早期引导、.bss初始化
│  │  ├─ start.c      # PMP、委托、mtvec=timer_vector、mret→S
│  │  └─ main.c       # 初始化、virtio_init()、proc_init()、proc_make_first()
│  ├─ proc/
│  │  ├─ proc.c       # 进程管理（init/alloc/free/fork/exit/wait/scheduler）
│  │  ├─ exec.c       # ELF 文件加载（proc_exec）
│  │  └─ swtch.S      # 上下文切换
│  ├─ trap/
│  │  ├─ trap.S       # S 态内核trap入口 kernel_vector
│  │  ├─ trap_kernel.c# 内核trap处理（时钟中断、VirtIO中断）
│  │  ├─ trampoline.S # 用户态trap入口 user_vector, user_return
│  │  └─ trap_user.c  # 用户态trap处理
│  ├─ mem/
│  │  ├─ kvm.c        # 页表管理、VirtIO MMIO映射、用户内存访问
│  │  └─ pmem.c       # 物理内存分配
│  ├─ syscall/
│  │  ├─ syscall.c    # 系统调用分发器、参数提取
│  │  ├─ sysproc.c    # 进程相关系统调用（fork/exit/wait/sleep/print/brk/mmap/exec）
│  │  └─ sysfile.c    # 文件相关系统调用（open/close/read/write）
│  ├─ dev/
│  │  ├─ virtio.c     # VirtIO 磁盘驱动（初始化、中断处理、块读写）
│  │  ├─ timer.c      # 时钟中断处理
│  │  └─ uart.c       # UART 驱动
│  ├─ fs/
│  │  ├─ buf.c        # Buf Cache（LRU + Lazy Write）
│  │  ├─ fs.c          # 文件系统初始化（super block读取、inode读写自测）
│  │  ├─ bitmap.c     # Bitmap 管理（balloc/bfree/ialloc/ifree）
│  │  ├─ inode.c       # Inode 层（数据索引、读写操作）
│  │  ├─ dir.c         # 目录操作（dir_add_entry/dir_lookup/path_to_inode）
│  │  └─ file.c        # 文件结构管理（file_alloc/file_open/file_read/file_write）
│  ├─ lib/            # 工具库（print、spinlock）
│  ├─ kernel.ld       # 链接脚本（包含trampoline段）
│  └─ Makefile        # 子目录构建与链接
├─ user/
│  ├─ user.h           # 用户态系统调用函数声明
│  ├─ syscall.h        # 用户态系统调用号定义
│  ├─ sys.h            # 通用系统调用包装函数
│  ├─ usys.pl          # 生成用户态系统调用桩代码
│  └─ test.c           # 测试程序
├─ Makefile            # 顶层构建与运行（qemu、fs.img生成）
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
   - CPU0：`pmem_init()` → `kvm_init()` → `trap_kernel_init()` → `virtio_init()` → `proc_init()`；
   - 所有CPU：`kvm_inithart()` → `trap_kernel_inithart()`；
   - 其他CPU：死循环（或运行调度器）；
   - CPU0：`proc_make_first()` 创建proczero并切换到用户态。

4. **fork_return()（proc.c）**：
   - 第一次返回用户态前：调用 `buf_init()` → `fs_init()` 初始化文件系统；
   - `fs_init()` 中会运行 inode 读写自测（打印 success/fail 后无限循环）。

5. **virtio.c（磁盘驱动）**：
   - `virtio_init()`：初始化 VirtIO MMIO 设备、配置队列、使能设备；
   - `virtio_intr()`：处理磁盘中断；
   - `virtio_disk_rw()`：执行块读写操作（支持 1024 字节块）。

6. **buf.c（Buf Cache）**：
   - `buf_init()`：初始化双向循环链表（已分配链/可分配链）；
   - `buf_read()`：读取块（LRU策略，命中时移动到最近使用端）；
   - `buf_write()`：标记 dirty（懒惰写回）；
   - `buf_release()`：释放引用（ref==0 时移动到可分配链）。

7. **fs.c（文件系统初始化）**：
   - `fs_init()`：读取 super block、初始化 inode 缓存、运行 inode 读写自测；
   - super block 包含：魔数、文件系统大小、块数、inode数、各区域起始块号。

8. **bitmap.c（Bitmap 管理）**：
   - `balloc()` / `bfree()`：数据块分配/释放（使用数据 bitmap）；
   - `ialloc()` / `ifree()`：inode 分配/释放（使用 inode bitmap）；
   - bitmap 各占 1 个 block，每个 bit 表示一个块/inode 的占用状态。

9. **inode.c（Inode 层）**：
   - `inode_alloc()`：分配新 inode（调用 ialloc，初始化 inode 结构）；
   - `inode_get()` / `inode_put()`：inode 缓存管理（引用计数）；
   - `inode_locate_block()`：定位逻辑块对应的物理块（10+2*N+N*N 索引结构）；
   - `inode_read_data()` / `inode_write_data()`：跨块读写数据。

10. **dir.c（目录操作）**：
    - `dir_add_entry()`：在目录中添加条目（dirent{inum, name}）；
    - `dir_lookup()`：在目录中查找条目；
    - `path_to_pinode()` / `path_to_inode()`：路径解析（返回父目录 inode / 最终 inode）。

11. **file.c（文件结构管理）**：
    - `file_alloc()` / `file_close()`：文件结构分配/释放；
    - `file_open()`：打开文件（解析路径、创建文件结构、分配文件描述符）；
    - `file_read()` / `file_write()`：文件读写（调用 inode_read_data/inode_write_data）。

12. **sysfile.c（文件系统调用）**：
    - `sys_open()`：打开文件（分配文件描述符）；
    - `sys_read()` / `sys_write()`：读写文件（用户空间 ↔ 内核空间数据拷贝）；
    - `sys_close()`：关闭文件（释放文件描述符）；
    - `sys_exec()`：执行 ELF 文件（调用 proc_exec）。

13. **exec.c（ELF 文件加载）**：
    - `proc_exec()`：解析 ELF 文件头、加载程序段到内存、设置用户栈、设置入口地址。

14. **用户态执行**：
    - 执行 `initcode[]` 或从文件系统加载的用户程序；
    - 通过系统调用进行文件操作、进程操作等。

---

## 4. 验证

> 完整细节与截图见 `report.md` 的"测试验证部分"。

### 4.1 验收标准
- ✅ QEMU 成功挂载文件系统磁盘映像（fs.img）
- ✅ VirtIO 磁盘驱动正常工作（初始化、中断处理、块读写）
- ✅ Buf Cache 正常工作（LRU 策略、懒惰写回）
- ✅ 文件系统初始化成功（super block 读取、inode 缓存初始化）
- ✅ Bitmap 管理正常工作（块和 inode 的分配与回收）
- ✅ Inode 层正常工作（数据索引、跨块读写）
- ✅ 目录和路径解析正常工作
- ✅ 文件系统调用正常工作（open/read/write/close）
- ✅ ELF 文件加载正常工作（proc_exec）
- ✅ Inode 读写自测打印 "success"
- ✅ 系统不 panic、不 page fault

### 4.2 预期输出
```
# 系统启动和初始化信息...
# virtio: disk initialized
# fs_init: file system initialized
# inode 1: type=2, size=0, nlink=1
#   addrs: 
# inode 1: type=2, size=2048, nlink=1
#   addrs: <block numbers>
# success
# （然后无限循环）
```

### 4.3 文件系统测试
已实现的功能包括：
- ✅ **VirtIO 磁盘驱动**：MMIO 映射、中断处理、块读写
- ✅ **Buf Cache**：LRU 缓存、懒惰写回
- ✅ **文件系统初始化**：super block 读取、inode 缓存初始化
- ✅ **Bitmap 管理**：balloc/bfree、ialloc/ifree
- ✅ **Inode 层**：数据索引（10+2*N+N*N）、跨块读写
- ✅ **目录操作**：dir_add_entry、dir_lookup、path_to_inode
- ✅ **文件系统调用**：open/read/write/close
- ✅ **ELF 文件加载**：proc_exec

可以通过 inode 读写自测验证文件系统核心功能。

---

## 5. 关键实现

### 5.1 VirtIO 磁盘驱动

#### 5.1.1 硬件地址映射
- `VIRTIO_BASE = 0x10001000`：VirtIO MMIO 基地址
- `VIRTIO_IRQ = 1`：VirtIO 中断号
- 在 `kvm_init()` 中映射 VirtIO_BASE 到内核页表

#### 5.1.2 设备初始化（virtio_init）
1. 检查 magic 值（0x74726976）
2. 检查版本（version == 2）
3. 检查设备ID（VIRTIO_ID_BLOCK == 2）
4. 设置设备状态（ACKNOWLEDGE → DRIVER → FEATURES_OK → DRIVER_OK）
5. 配置队列（描述符、可用环、已用环）
6. 使能队列

#### 5.1.3 中断处理（virtio_intr）
- 在 `trap_kernel_handler()` 中处理 VIRTIO_IRQ
- 读取中断状态，确认中断，处理队列完成事件

#### 5.1.4 块读写（virtio_disk_rw）
- 支持 1024 字节块（每个块包含 2 个 512 字节 sector）
- 使用描述符链：请求头 + 数据 + 状态
- 轮询方式等待完成（简化实现）

### 5.2 Buf Cache

#### 5.2.1 数据结构
- 全局 buf 数组：`bufs[N_BLOCK_BUF]`（N_BLOCK_BUF = 6）
- 双向循环链表：
  - `head_buf->next`：已分配链（有 ref 的 buf）
  - `head_buf->prev`：可分配链（ref==0 的 buf，LRU 顺序）

#### 5.2.2 LRU 策略
- `buf_read()`：命中时移动到已分配链的最近使用端（head_buf->next）
- `buf_release()`：ref==0 时移动到可分配链（head_buf->prev，最久未使用在前）

#### 5.2.3 懒惰写回
- `buf_write()`：只标记 `buf->disk = 1`（dirty），不立即写磁盘
- 在 `buf_read()` 分配新 buf 时，如果原 buf 有效且 dirty，先写回

### 5.3 文件系统初始化

#### 5.3.1 Super Block
```c
struct super_block {
    uint32 magic;           // 文件系统魔数（0x12345678）
    uint32 size;            // 文件系统大小（块数）
    uint32 nblocks;         // 数据块数
    uint32 ninodes;         // inode数
    uint32 inodestart;     // inode起始块
    uint32 bmapstart;      // 数据bitmap起始块
    uint32 datastart;      // 数据块起始块
};
```

#### 5.3.2 磁盘布局
```
[super block | inode bitmap | inode blocks | data bitmap | data blocks]
```

#### 5.3.3 初始化流程
1. 读取 super block（SB_BLOCK = 1）
2. 检查魔数
3. 初始化 inode 缓存
4. 运行 inode 读写自测
5. 初始化文件系统（file_init）

### 5.4 Bitmap 管理

#### 5.4.1 数据块分配（balloc）
1. 读取数据 bitmap（bmapstart）
2. 查找第一个空闲位（bit = 0）
3. 设置位（bit = 1）
4. 返回物理块号（datastart + bit）

#### 5.4.2 Inode 分配（ialloc）
1. 读取 inode bitmap（bmapstart - 1）
2. 查找第一个空闲位
3. 设置位
4. 返回 inode 号（bit + 1，inode 从 1 开始）

### 5.5 Inode 层

#### 5.5.1 Inode 结构
```c
struct dinode {
    uint16 type;           // 文件类型（T_DIR/T_FILE/T_DEV）
    uint16 major;          // 主设备号
    uint16 minor;          // 次设备号
    uint16 nlink;          // 链接数
    uint32 size;           // 文件大小（字节）
    uint32 addrs[12];      // 数据块地址
};
```

#### 5.5.2 数据索引结构（10+2*N+N*N）
- **直接索引**：`addrs[0..9]`（10 个直接块）
- **一级间接索引**：`addrs[10]`（指向包含 256 个块号的块）
- **二级间接索引**：`addrs[11]`（指向包含 256 个一级间接块的块）

#### 5.5.3 块定位（inode_locate_block）
1. 如果 `bn < 10`：直接返回 `addrs[bn]`（如果为 0 则分配）
2. 如果 `bn < 266`：使用一级间接索引
3. 如果 `bn >= 266`：使用二级间接索引（简化实现暂不支持）

#### 5.5.4 数据读写（inode_read_data/inode_write_data）
- 支持跨块读写
- 按块对齐处理，使用 buf cache 读写数据

### 5.6 目录操作

#### 5.6.1 目录项结构
```c
struct dirent {
    uint16 inum;
    char name[DIRSIZ];  // DIRSIZ = 14
};
```

#### 5.6.2 目录操作
- `dir_add_entry()`：在目录中添加条目（查找空闲槽位或追加）
- `dir_lookup()`：在目录中查找条目（遍历目录项）

#### 5.6.3 路径解析
- `path_to_pinode()`：解析路径，返回父目录 inode 和最后一级名字
- `path_to_inode()`：解析路径，返回最终 inode

### 5.7 文件系统调用

#### 5.7.1 文件描述符管理
- 每个进程有 `ofile[16]` 数组
- `fdalloc()`：分配文件描述符（查找空闲槽位）

#### 5.7.2 sys_open
1. 解析路径（`path_to_inode`）
2. 如果文件不存在且 O_CREAT：创建新文件（`inode_alloc` + `dir_add_entry`）
3. 分配文件结构（`file_alloc`）
4. 分配文件描述符（`fdalloc`）

#### 5.7.3 sys_read/sys_write
1. 从文件描述符获取文件结构
2. 使用临时缓冲区（512 字节）
3. 调用 `file_read` / `file_write`
4. 使用 `copyout` / `copyin` 在用户空间和内核空间之间拷贝数据

#### 5.7.4 sys_close
1. 从文件描述符获取文件结构
2. 清空 `ofile[fd]`
3. 调用 `file_close`（减少引用计数，必要时释放 inode）

### 5.8 ELF 文件加载（proc_exec）

#### 5.8.1 ELF 文件头解析
- 检查 ELF 魔数（0x464c457f）
- 检查机器类型（EM_RISCV = 0xF3）

#### 5.8.2 程序段加载
1. 释放旧的用户页表
2. 创建新的用户页表
3. 遍历程序头（program headers）
4. 对于每个 PT_LOAD 段：
   - 分配物理页并映射到用户地址空间
   - 从文件读取数据到内存
   - 清零未初始化的部分

#### 5.8.3 设置用户栈和入口
- 分配用户栈页
- 设置 `tf->epc = entry`（程序入口地址）
- 设置 `tf->sp`（用户栈指针）

---

## 6. Inode 读写自测

### 6.1 测试位置
- 在 `fs_init()` 中，`inode_init()` 之后运行
- 内核态自测，不依赖用户程序

### 6.2 测试流程
1. 创建测试数据（0 到 2*BLOCK_SIZE-1）
2. 创建新的 inode（类型为 T_FILE）
3. 第一次写入：从偏移 0 写入 BLOCK_SIZE/2 字节
4. 第二次写入：从偏移 BLOCK_SIZE/2 写入 BLOCK_SIZE + BLOCK_SIZE/2 字节
5. 读取：从偏移 0 读取 BLOCK_SIZE * 2 字节
6. 比较数据，打印 "success" 或 "fail"
7. 无限循环（用于验证结果）

### 6.3 预期输出
```
inode 1: type=2, size=0, nlink=1
  addrs: 
inode 1: type=2, size=2048, nlink=1
  addrs: <block numbers>
success
```

---

## 7. 参考运行命令

```bash
# 编译和运行
make clean && make build && make qemu

# 预期行为
# - 系统启动并初始化
# - VirtIO 磁盘驱动初始化
# - 文件系统初始化（读取 super block）
# - 运行 inode 读写自测
# - 打印 "success" 后无限循环
```

---

## 8. 实验七新增功能总结

### 8.1 VirtIO 磁盘驱动
- ✅ MMIO 地址映射（VIRTIO_BASE）
- ✅ 中断处理（VIRTIO_IRQ）
- ✅ 设备初始化（virtio_init）
- ✅ 块读写（virtio_disk_rw，支持 1024 字节块）

### 8.2 Buf Cache
- ✅ LRU 缓存策略（双向循环链表）
- ✅ 懒惰写回机制
- ✅ 引用计数管理

### 8.3 文件系统初始化
- ✅ Super block 读取
- ✅ Inode 缓存初始化
- ✅ Inode 读写自测

### 8.4 Bitmap 管理
- ✅ 数据块分配/释放（balloc/bfree）
- ✅ Inode 分配/释放（ialloc/ifree）

### 8.5 Inode 层
- ✅ Inode 缓存管理（inode_get/inode_put）
- ✅ 数据索引（10+2*N+N*N 结构）
- ✅ 跨块读写（inode_read_data/inode_write_data）

### 8.6 目录操作
- ✅ 目录项管理（dir_add_entry/dir_lookup）
- ✅ 路径解析（path_to_pinode/path_to_inode）

### 8.7 文件系统调用
- ✅ sys_open（打开文件、创建文件）
- ✅ sys_read/sys_write（文件读写）
- ✅ sys_close（关闭文件）
- ✅ sys_exec（执行 ELF 文件）

### 8.8 ELF 文件加载
- ✅ ELF 文件头解析
- ✅ 程序段加载
- ✅ 用户栈设置

---

## 9. 注意事项

1. **文件系统映像**：
   - 首次运行会自动创建空的 `fs.img`（10MB）
   - 如需使用有效的文件系统，需要先格式化文件系统映像（创建 super block、bitmap 等）

2. **Super Block 魔数**：
   - 当前使用 `0x12345678` 作为魔数
   - 需要与实际文件系统映像匹配

3. **Inode 读写自测**：
   - 测试代码在 `while(1)` 处会无限循环
   - 这是预期行为，用于验证测试结果

4. **文件系统布局**：
   - Super block 在块 1（SB_BLOCK = 1）
   - Inode bitmap 在 bmapstart - 1
   - 数据 bitmap 在 bmapstart
   - Inode blocks 在 inodestart
   - 数据 blocks 在 datastart

---
