#include "proc/proc.h"
#include "fs/file.h"
#include "fs/inode.h"
#include "mem/kvm.h"
#include "mem/pmem.h"
#include "lib/print.h"
#include "mem/str.h"

// ELF魔数
#define ELF_MAGIC 0x464c457f  // "\x7fELF"

// ELF文件头
struct elfhdr {
    uint32 magic;
    uint8 elf[12];
    uint16 type;
    uint16 machine;
    uint32 version;
    uint64 entry;
    uint64 phoff;
    uint64 shoff;
    uint32 flags;
    uint16 ehsize;
    uint16 phentsize;
    uint16 phnum;
    uint16 shentsize;
    uint16 shnum;
    uint16 shstrndx;
};

// 程序头
struct proghdr {
    uint32 type;
    uint32 flags;
    uint64 off;
    uint64 vaddr;
    uint64 paddr;
    uint64 filesz;
    uint64 memsz;
    uint64 align;
};

// 从文件读取数据到内核缓冲区
static int readi(struct file* f, void* dst, uint64 off, uint64 n) {
    // 保存当前偏移
    uint32 old_off = f->off;
    f->off = off;
    
    // 读取数据
    int r = file_read(f, dst, n);
    
    // 恢复偏移
    f->off = old_off;
    
    return r;
}

// 执行ELF文件
int proc_exec(const char* path) {
    proc_t *p = myproc();
    struct file *f;
    struct elfhdr elf;
    struct proghdr ph;
    
    // 打开文件
    f = file_open(path, 0);  // O_RDONLY
    if (f == NULL) {
        return -1;
    }
    
    // 读取ELF头
    if (readi(f, &elf, 0, sizeof(elf)) != sizeof(elf)) {
        file_close(f);
        return -1;
    }
    
    // 检查ELF魔数
    if (elf.magic != ELF_MAGIC) {
        file_close(f);
        return -1;
    }
    
    // 检查机器类型（RISC-V）
    if (elf.machine != 0xF3) {  // EM_RISCV
        file_close(f);
        return -1;
    }
    
    // 释放旧的用户页表
    if (p->pgtbl) {
        // 释放用户地址空间的所有映射
        vm_unmappages(p->pgtbl, 0, 0x80000000UL, true);
        pmem_free(p->pgtbl, true);
    }
    
    // 创建新的用户页表
    p->pgtbl = proc_pgtbl_init((uint64)p->tf);
    if (p->pgtbl == NULL) {
        file_close(f);
        return -1;
    }
    
    // 加载每个程序段
    uint64 entry = 0;
    for (int i = 0; i < elf.phnum; i++) {
        // 读取程序头
        if (readi(f, &ph, elf.phoff + i * sizeof(ph), sizeof(ph)) != sizeof(ph)) {
            file_close(f);
            return -1;
        }
        
        // 只处理LOAD类型的段
        if (ph.type != 1) {  // PT_LOAD
            continue;
        }
        
        if (i == 0) {
            entry = ph.vaddr;
        }
        
        // 分配物理页并映射
        uint64 va = ph.vaddr;
        uint64 sz = ph.memsz;
        
        // 对齐到页边界
        uint64 va_start = (va / PGSIZE) * PGSIZE;
        uint64 va_end = ((va + sz + PGSIZE - 1) / PGSIZE) * PGSIZE;
        
        for (uint64 a = va_start; a < va_end; a += PGSIZE) {
            void* pa = pmem_alloc(true);
            if (pa == NULL) {
                file_close(f);
                return -1;
            }
            
            // 映射到用户地址空间
            vm_mappages(p->pgtbl, a, (uint64)pa, PGSIZE, PTE_R | PTE_W | PTE_X | PTE_U);
            
            // 从文件读取数据
            if (a < va + ph.filesz) {
                uint64 off = a - va;
                uint64 n = PGSIZE;
                if (off + n > ph.filesz) {
                    n = ph.filesz - off;
                }
                if (readi(f, (char*)pa + (a - va_start), ph.off + off, n) < 0) {
                    file_close(f);
                    return -1;
                }
            } else {
                // 清零未初始化的部分
                memset((char*)pa + (a - va_start), 0, PGSIZE);
            }
        }
    }
    
    file_close(f);
    
    // 设置用户栈
    uint64 stack_va = 0x80000000UL - PGSIZE;
    void* stack_pa = pmem_alloc(true);
    if (stack_pa == NULL) {
        return -1;
    }
    memset(stack_pa, 0, PGSIZE);
    vm_mappages(p->pgtbl, stack_va, (uint64)stack_pa, PGSIZE, PTE_R | PTE_W | PTE_U);
    
    // 设置trapframe
    p->tf->epc = entry;
    p->tf->sp = stack_va + PGSIZE;
    
    return 0;
}

