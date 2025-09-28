#include "../include/types.h"
#include "../include/riscv.h"
#include "../include/pmm.h"
#include "../include/vm.h"
#include "../include/memlayout.h"
#include "../include/string.h"
#include "../include/printf.h"
#include "../include/stddef.h"

// 声明由链接脚本提供的符号
extern char etext[]; // 内核代码段(.text)的结束地址

// 内部辅助函数声明
pte_t* walk_create(pagetable_t pt, uint64 va);
void free_walk(pagetable_t pt);
void dump_walk(pagetable_t pt, int level, uint64 current_va);

// 内部辅助函数：映射一段内存区域
void kvmmap(pagetable_t kpt, uint64 va, uint64 pa, uint64 sz, int perm) {
    if(map_page(kpt, va, pa, perm) != 0) {
        panic("kvmmap");
    }
}

// 创建页表结构
// kernel_pagetable 是内核页表的指针
void kvminit(void) {
    printf("kvminit: creating kernel pagetable...\n");
    // 创建一个空的根页表
    kernel_pagetable = create_pagetable();
    if (!kernel_pagetable) {
        panic("kvminit: cannot create kernel pagetable");
    }

    // --- 映射设备 ---
    // 映射 UART 寄存器，确保在启用分页后，printf还能通过这个地址访问到串口硬件
    // 权限为可读+可写
    printf("kvminit: mapping UART...\n");
    map_page(kernel_pagetable, 0x10000000, 0x10000000, PTE_R | PTE_W);

    // --- 映射内核 ---
    // 映射内核代码段 (.text)，权限为 可读(R) + 可执行(X)，不可写保护内核代码
    printf("kvminit: mapping kernel .text (R-X)...\n");
    map_page(kernel_pagetable, KERNBASE, KERNBASE, PTE_R | PTE_X);

    // 映射内核数据段 (.data) 和之后的所有物理内存
    // 权限为 可读(R) + 可写(W)
    printf("kvminit: mapping kernel .data and physical memory (RW-)...\n");
    // 这里简化处理，直接将从etext到PHYSTOP的整个范围映射
    // 在真实的xv6中会更精细
    uint64 pa_start = PGROUNDUP((uint64)etext);
    for(uint64 pa = pa_start; pa < PHYSTOP; pa += PGSIZE) {
        map_page(kernel_pagetable, pa, pa, PTE_R | PTE_W);
    }

    printf("kvminit: kernel pagetable created.\n");
}

// 激活页表，开启分页模式
void kvminithart(void) {
    printf("kvminithart: enabling paging...\n");
    w_satp(MAKE_SATP(kernel_pagetable));
    // 刷新TLB
    sfence_vma();
    printf("kvminithart: paging enabled.\n");
}

/* 下面均为辅助函数 */
// 页表管理工具
// 分配一个物理页作为根页表
pagetable_t create_pagetable(void) {
    return (pagetable_t)alloc_page();
}

// 将一个虚拟页映射到一个物理页
int map_page(pagetable_t pt, uint64 va, uint64 pa, int perm) {
    if (va % PGSIZE != 0 || pa % PGSIZE != 0) {
        panic("map_page: unaligned addresses");
    }

    pte_t *pte = walk_create(pt, va);
    if (pte == 0) return -1;

    if (*pte & PTE_V) {
        panic("map_page: remap");
    }
    
    *pte = PA2PTE(pa) | perm | PTE_V;
    return 0;
}

// 销毁一个完整的页表
void destroy_pagetable(pagetable_t pt) {
    if (pt == NULL) return;
    free_walk(pt);
}

// 查找现有映射，如果有效就返回地址，无效就返回0
pte_t* walk_lookup(pagetable_t pt, uint64 va) {
    if(va >= (1L << 39)) {
        return 0;
    }

    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pt[PX(level, va)];
        if (*pte & PTE_V) {
            // PTE有效，获取下一级页表的物理地址
            pt = (pagetable_t)PTE2PA(*pte);
        } else {
            // PTE无效，意味着映射不存在，查找失败
            return 0;
        }
    }
    // 返回最末级（L0）PTE的地址
    return &pt[PX(0, va)];
}

// 查找现有映射，如果有效就返回地址，无效就创建一个新的页进行映射
pte_t* walk_create(pagetable_t pt, uint64 va) {
    if(va >= (1L << 39)) panic("walk_create: va too large");

    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pt[PX(level, va)];
        if (*pte & PTE_V) {
            pt = (pagetable_t)PTE2PA(*pte);
        } else {
            pt = (pagetable_t)alloc_page();
            if (pt == 0) return 0;
            memset(pt, 0, PGSIZE);
            *pte = PA2PTE(pt) | PTE_V;
        }
    }
    return &pt[PX(0, va)];
}

// 释放一个页表以及它所指向的所有子页表
void free_walk(pagetable_t pt) {
    for (int i = 0; i < 512; i++) {
        pte_t pte = pt[i];
        // 一个指向下一级页表的PTE（中间PTE），它的R/W/X位必须全部为0
        if ((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
            uint64 child = PTE2PA(pte);
            free_walk((pagetable_t)child);
        }
    }
    free_page((void*)pt);
}

void dump_walk(pagetable_t pt, int level, uint64 current_va) {
    if(level < 0) return;
    for (int i = 0; i < 512; i++) {
        pte_t pte = pt[i];
        if (pte & PTE_V) {
            uint64 child_va = current_va + ((uint64)i << (12 + level * 9));
            for(int j = 2; j > level; j--) printf("  ");
            
            printf("L%d[%03d]: va=0x%lx -> pa=0x%lx | %c%c%c%c%c\n", 
                   level, i, child_va, PTE2PA(pte),
                   (pte & PTE_V) ? 'V' : '-', (pte & PTE_R) ? 'R' : '-',
                   (pte & PTE_W) ? 'W' : '-', (pte & PTE_X) ? 'X' : '-',
                   (pte & PTE_U) ? 'U' : '-');
            
            if ((pte & (PTE_R|PTE_W|PTE_X)) == 0) { // 中间PTE
                dump_walk((pagetable_t)PTE2PA(pte), level - 1, child_va);
            }
        }
    }
}

void dump_pagetable(pagetable_t pt) {
    printf("--- Pagetable Dump (Root at PA: 0x%lx) ---\n", (uint64)pt);
    dump_walk(pt, 2, 0);
    printf("--- End of Dump ---\n");
}