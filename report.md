# LAB-2: 内存管理实验综合报告

代码仓库：https://github.com/WHUer1688/riscv-os-lab/tree/Lab-2

---

## 1. 系统设计部分

### 1.1 架构设计说明

本实验在LAB-1的基础上，实现了完整的物理和虚拟内存管理系统。系统采用模块化设计，主要包含以下核心组件：


#### 内存管理模块设计

**1. 物理内存管理 (pmem.c)**
- **分离式内存区域**: 将物理内存分为内核区域和用户区域，独立管理
- **空闲链表管理**: 使用侵入式链表管理空闲物理页面
- **引用计数机制**: 支持页面共享，实现写时复制(COW)的基础
- **多核同步**: 使用自旋锁保护临界区，支持多核并发分配

**2. 虚拟内存管理 (kvm.c)**
- **三级页表结构**: 遵循RISC-V Sv39分页方案
- **动态页表创建**: 按需分配中间页表
- **灵活的权限控制**: 支持R/W/X权限组合
- **页表映射/取消映射**: 完整的虚拟地址空间管理

**3. 字符串操作 (str.c)**
- **内存操作**: memset, memcpy, memmove
- **字符串处理**: strlen, strcpy, strcmp

#### 启动流程

```
entry.S (汇编入口)
    ↓
start.c (C语言启动)
    ↓
main.c (主程序)
    ↓
pmem_init() → kvm_init() → kvm_inithart()
    ↓
测试代码 (页表映射/取消映射/引用计数)
```

### 1.2 关键数据结构

#### 1. 物理内存管理数据结构

##### 页面链表节点
```c
typedef struct page_node {
    struct page_node* next;  // 指向下一个空闲页面
} page_node_t;
```
**设计思想**: 侵入式链表，直接利用空闲页面的头部空间存储链表指针，无需额外内存开销。

##### 可分配区域
```c
typedef struct alloc_region {
    uint64 begin;           // 起始物理地址
    uint64 end;             // 终止物理地址  
    spinlock_t lk;          // 保护并发访问的自旋锁
    uint32 allocable;       // 可分配页面计数
    page_node_t list_head;  // 空闲链表头节点
} alloc_region_t;
```
**设计要点**:
- **分离管理**: 内核和用户区域各有独立的 `alloc_region_t`
- **快速分配**: 从链表头部O(1)时间分配
- **统计信息**: `allocable` 字段实时跟踪可用页面数


#### 2. 虚拟内存管理数据结构

##### 页表和页表项
```c
typedef uint64 pte_t;     // 页表项（64位）
typedef pte_t* pgtbl_t;   // 页表指针
```

##### 页表项标志位
```c
#define PTE_V (1L << 0)   // 有效位
#define PTE_R (1L << 1)   // 可读
#define PTE_W (1L << 2)   // 可写
#define PTE_X (1L << 3)   // 可执行
#define PTE_U (1L << 4)   // 用户可访问
```

#### 3. 内存布局
```c
#define KERNEL_BASE    0x80000000L     // 内核起始地址
#define ALLOC_BEGIN    0x82000000L     // 可分配内存起始
#define ALLOC_END      0x88000000L     // 可分配内存结束
#define KERNEL_PAGES   512             // 内核页面数
#define PGSIZE         4096            // 页面大小(4KB)
#define MAXVA          (1L << 38)      // 最大虚拟地址(256GB)
```


### 1.3 设计决策理由

#### 1.3.1 分离式物理内存管理
**决策**: 将物理内存分为内核区域和用户区域，使用独立的 `alloc_region_t` 管理。

**理由**:
- **安全隔离**: 防止用户代码意外访问内核物理页


#### 1.3.2 侵入式空闲链表
**决策**: 直接在空闲物理页中存储链表指针。

**理由**:
- **零额外开销**: 不需要额外的内存来存储链表节点

#### 1.3.3 三级页表设计
**决策**: 采用RISC-V Sv39标准的三级页表结构。


---

## 2. 实验过程部分

### 2.1 实现步骤记录

#### 步骤1: 物理内存管理器实现 (pmem.c)

**1.1 数据结构设计**
```c
// 定义两个全局内存区域
static alloc_region_t kern_region, user_region;
// 引用计数数组
static uint32 refcount[MAX_PAGES];
static spinlock_t refcount_lk;
```

**1.2 初始化函数实现**
```c
void pmem_init(void) {
    // 初始化内核区域
    kern_region.begin = ALLOC_BEGIN;
    kern_region.end = ALLOC_BEGIN + KERNEL_PAGES * PGSIZE;
    // 初始化用户区域
    user_region.begin = ALLOC_BEGIN + KERNEL_PAGES * PGSIZE;
    user_region.end = ALLOC_END;
    // 初始化锁
    spinlock_init(&kern_region.lk, "kern_region");
    spinlock_init(&user_region.lk, "user_region");
    spinlock_init(&refcount_lk, "refcount");
}
```

**1.3 页面分配实现**
```c
void* pmem_alloc(bool in_kernel) {
    // 1. 选择区域
    alloc_region_t *region = in_kernel ? &kern_region : &user_region;
    
    // 2. 加锁
    spinlock_acquire(&region->lk);
    
    // 3. 延迟构建链表
    if(region->list_head.next == NULL) {
        for(uint64 pa = region->begin; pa < region->end; pa += PGSIZE) {
            page_node_t *page = (page_node_t*)pa;
            page->next = region->list_head.next;
            region->list_head.next = page;
        }
    }
    
    // 4. 从链表头部取出一页
    page_node_t *page = region->list_head.next;
    region->list_head.next = page->next;
    
    // 5. 解锁
    spinlock_release(&region->lk);
    
    // 6. 清零页面
    memset((void*)page, 0, PGSIZE);
    
    // 7. 设置引用计数为1
    refcount[pa_to_index((uint64)page)] = 1;
    
    return (void*)page;
}
```

**1.4 引用计数管理实现**
```c
// 增加引用
void pmem_incref(void* pa) {
    spinlock_acquire(&refcount_lk);
    refcount[pa_to_index(pa)]++;
    spinlock_release(&refcount_lk);
}

// 减少引用，计数为0时释放
void pmem_decref(void* pa, bool in_kernel) {
    spinlock_acquire(&refcount_lk);
    refcount[pa_to_index(pa)]--;
    uint32 count = refcount[pa_to_index(pa)];
    spinlock_release(&refcount_lk);
    
    if(count == 0) {
        pmem_free(pa, in_kernel);
    }
}
```

#### 步骤2: 虚拟内存管理器实现 (kvm.c)

**2.1 页表项获取实现**
```c
pte_t *vm_getpte(pgtbl_t pgtbl, uint64 va, bool alloc) {
    // 遍历三级页表
    for(int level = 2; level > 0; level--) {
        pte_t *pte = &pgtbl[PX(level, va)];
        if(*pte & PTE_V) {
            // 页表项有效，获取下一级页表
            pgtbl = (pgtbl_t)PTE2PA(*pte);
        } else {
            // 页表项无效，需要分配新页表
            if(!alloc) return NULL;
            pgtbl = (pgtbl_t)pmem_alloc(true);
            memset(pgtbl, 0, PGSIZE);
            *pte = PA2PTE(pgtbl) | PTE_V;
        }
    }
    // 返回叶子节点页表项指针
    return &pgtbl[PX(0, va)];
}
```

**2.2 页面映射实现**
```c
void vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm) {
    uint64 a = PG_ROUND_DOWN(va);
    uint64 last = PG_ROUND_DOWN(va + len - 1);
    
    for(;;) {
        pte_t *pte = vm_getpte(pgtbl, a, true);
        
        if(*pte & PTE_V) {
            // 已映射，检查是否映射到同一物理地址
            if(PTE2PA(*pte) != pa) {
                panic("remap");
            }
            // 更新权限，不增加引用计数
        } else {
            // 新映射，增加引用计数
            pmem_incref((void*)pa);
        }
        
        *pte = PA2PTE(pa) | perm | PTE_V;
        
        if(a == last) break;
        a += PGSIZE;
        pa += PGSIZE;
    }
}
```

**2.3 页面取消映射实现**
```c
void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit) {
    uint64 a = PG_ROUND_DOWN(va);
    uint64 last = PG_ROUND_DOWN(va + len - 1);
    
    for(;;) {
        pte_t *pte = vm_getpte(pgtbl, a, false);
        if(*pte & PTE_V) {
            uint64 pa = PTE2PA(*pte);
            *pte = 0;  // 清除页表项
            
            // 使用引用计数机制释放
            if(freeit)
                pmem_decref((void*)pa, true);
        }
        
        if(a == last) break;
        a += PGSIZE;
    }
}
```

**2.4 内核页表初始化实现**
```c
void kvm_init(void) {
    // 分配内核页表
    kernel_pgtbl = (pgtbl_t)pmem_alloc(true);
    memset(kernel_pgtbl, 0, PGSIZE);
    
    // 直接映射硬件设备
    vm_mappages(kernel_pgtbl, UART_BASE, UART_BASE, PGSIZE, PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, CLINT_BASE, CLINT_BASE, PGSIZE, PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, PLIC_BASE, PLIC_BASE, PGSIZE, PTE_R | PTE_W);
    
    // 直接映射内核代码和数据
    vm_mappages(kernel_pgtbl, KERNEL_BASE, KERNEL_BASE, 0x8000000, 
                PTE_R | PTE_W | PTE_X);
}

void kvm_inithart(void) {
    // 加载页表到satp寄存器
    w_satp(MAKE_SATP(kernel_pgtbl));
    // 刷新TLB
    sfence_vma();
}
```



### 2.2 问题与解决方案

#### 问题1: 未映射地址访问导致系统崩溃
**现象**: 在 `pmem_init()` 中构建空闲链表时系统崩溃。

**原因分析**:
- `pmem_init()` 在 `kvm_init()` 之前调用
- 此时虚拟内存尚未启用，直接访问物理地址导致异常
- 空闲链表需要写入物理页的头部，但这些地址未映射

**解决方案**:
```c
void* pmem_alloc(bool in_kernel) {
    // 延迟构建链表，只在第一次分配时构建
    if(region->list_head.next == NULL) {
        for(uint64 pa = region->begin; pa < region->end; pa += PGSIZE) {
            page_node_t *page = (page_node_t*)pa;
            page->next = region->list_head.next;
            region->list_head.next = page;
        }
    }
    // ... 继续分配逻辑
}
```

#### 问题2: 引用计数导致双重释放
**现象**: 取消映射后再次分配得到了刚释放的页面，但该页面的引用计数不为0。

**原因分析**:
- 最初实现中，`vm_unmappages()` 直接调用 `pmem_free()`
- 但物理页可能被多个虚拟地址映射（引用计数>1）
- 直接释放会导致其他映射访问已释放的内存


**解决方案**:
```c
void vm_unmappages(pgtbl_t pgtbl, uint64 va, uint64 len, bool freeit) {
    // ...
    if(freeit)
        pmem_decref((void*)pa, true);  // 使用引用计数机制
}
```

#### 问题3: 页表映射重复导致panic
**现象**: 测试代码中尝试更新权限时触发 "remap" panic。

**原因分析**:
- 测试代码先映射 `va → pa` (只读)
- 再次映射 `va → pa` (可写)，想更新权限
- 但代码检测到已存在映射，触发panic

**解决方案**:
```c
void vm_mappages(pgtbl_t pgtbl, uint64 va, uint64 pa, uint64 len, int perm) {
    // ...
    if(*pte & PTE_V) {
        // 检查物理地址是否相同
        uint64 old_pa = PTE2PA(*pte);
        if(old_pa != pa) {
            panic("remap");  // 映射到不同物理地址，报错
        }
        // 映射到相同物理地址，允许更新权限，不增加引用计数
    } else {
        // 新映射，增加引用计数
        pmem_incref((void*)pa);
    }
    *pte = PA2PTE(pa) | perm | PTE_V;
}
```

## 3. 测试验证部分

### 3.1 功能测试结果

#### 3.1.1 基础启动测试
**测试目标**: 验证系统能够正确初始化内存管理模块并启动多核。

**测试结果**:

![启动测试](picture/start_test.png)

**结论**: 所有三个CPU成功启动，内存管理模块正常初始化。

#### 3.1.2 页表映射测试
**测试目标**: 验证虚拟地址到物理地址的映射功能。

**测试结果**:

![启动测试](picture/start_test.png)
