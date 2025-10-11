#include "riscv.h"
#include "lib/print.h"
#include "mem/pmem.h"
#include "mem/kvm.h"
#include "mem/str.h"

volatile static int started = 0;

int main()
{
    int cpuid = r_tp();

    if(cpuid == 0) {

        print_init();
        pmem_init();
        kvm_init();
        kvm_inithart();

        printf("cpu %d is booting!\n", cpuid);
        __sync_synchronize();
        started = 1;

        pgtbl_t test_pgtbl = pmem_alloc(true);
        uint64 mem[5];
        for(int i = 0; i < 5; i++)
            mem[i] = (uint64)pmem_alloc(false);

        printf("\ntest-1\n\n");    
        vm_mappages(test_pgtbl, 0, mem[0], PGSIZE, PTE_R);
        vm_mappages(test_pgtbl, PGSIZE * 10, mem[1], PGSIZE / 2, PTE_R | PTE_W);
        vm_mappages(test_pgtbl, PGSIZE * 512, mem[2], PGSIZE - 1, PTE_R | PTE_X);
        vm_mappages(test_pgtbl, PGSIZE * 512 * 512, mem[2], PGSIZE, PTE_R | PTE_X);
        vm_mappages(test_pgtbl, VA_MAX - PGSIZE, mem[4], PGSIZE, PTE_W);
        vm_print(test_pgtbl);

        printf("\ntest-2\n\n");    
        vm_mappages(test_pgtbl, 0, mem[0], PGSIZE, PTE_W);
        vm_unmappages(test_pgtbl, PGSIZE * 10, PGSIZE, true);
        vm_unmappages(test_pgtbl, PGSIZE * 512, PGSIZE, true);
        vm_print(test_pgtbl);

        /*
        // 回归测试：验证引用计数机制
        printf("\ntest-3: Reference counting test\n\n");
        
        // 1. 分配一个物理页
        uint64 test_pa = (uint64)pmem_alloc(false);
        printf("Allocated PA: %p, refcount = %d\n", test_pa, pmem_getref((void*)test_pa));
        
        // 2. 将同一物理页映射到两个不同的虚拟地址
        uint64 va1 = PGSIZE * 1000;
        uint64 va2 = PGSIZE * 2000;
        vm_mappages(test_pgtbl, va1, test_pa, PGSIZE, PTE_R | PTE_W);
        vm_mappages(test_pgtbl, va2, test_pa, PGSIZE, PTE_R | PTE_W);
        printf("Mapped to VA1 and VA2, refcount = %d (expected: 3)\n", pmem_getref((void*)test_pa));
        
        // 3. 取消一个映射（free=true）
        vm_unmappages(test_pgtbl, va1, PGSIZE, true);
        printf("Unmapped VA1, refcount = %d (expected: 2)\n", pmem_getref((void*)test_pa));
        
        // 4. 分配新页，验证没有复用刚才的物理页
        uint64 new_pa = (uint64)pmem_alloc(false);
        if(new_pa == test_pa) {
            printf("ERROR: New allocation reused the same PA! (double-free bug)\n");
        } else {
            printf("PASS: New PA %p is different from test PA\n", new_pa);
        }
        
        // 5. 取消第二个映射
        vm_unmappages(test_pgtbl, va2, PGSIZE, true);
        uint32 ref_after_unmap = pmem_getref((void*)test_pa);
        printf("Unmapped VA2, refcount = %d (expected: 1 - initial allocation ref)\n", ref_after_unmap);
        
        // 6. 手动减少初始分配的引用计数（模拟完全释放）
        pmem_decref((void*)test_pa, false);
        printf("Manually released initial ref, refcount = %d (expected: 0)\n", pmem_getref((void*)test_pa));
        
        // 7. 再次分配，验证可以获取新页面
        uint64 final_pa = (uint64)pmem_alloc(false);
        printf("Final allocation PA: %p\n", final_pa);
        
        printf("\nAll tests PASSED! Reference counting works correctly.\n");
        */

    } else {

        while(started == 0);
        __sync_synchronize();
        printf("cpu %d is booting!\n", cpuid);
         
    }
    while (1);    
}