#include "../include/types.h"
#include "../include/riscv.h"
#include "../include/pmm.h"
#include "../include/vm.h"
#include "../include/memlayout.h"
#include "../include/string.h"
#include "../include/printf.h"
#include "../include/stddef.h"
#include "../include/proc.h"

// 声明由链接脚本提供的符号
extern char etext[]; // 内核代码段(.text)的结束地址
// 声明外部定义的蹦床代码 (在 trampoline.S 中，如果你还没有，后面会说)
// extern char trampoline[];
pagetable_t kernel_pagetable;

// 内部辅助函数声明
pte_t* walk_create(pagetable_t pt, uint64 va);
void free_walk(pagetable_t pt);
void dump_walk(pagetable_t pt, int level, uint64 current_va);


// 内部辅助函数：映射一段内存区域
// void kvmmap(pagetable_t kpt, uint64 va, uint64 pa, uint64 sz, int perm) {
//     if(map_page(kpt, va, pa, perm) != 0) {
//         panic("kvmmap");
//     }
// }
void kvmmap(pagetable_t kpt, uint64 va, uint64 pa, uint64 sz, int perm) {
    uint64 a = PGROUNDDOWN(va);
    uint64 last = PGROUNDDOWN(va + sz - 1);

    for(;;){
        if(map_page(kpt, a, pa, perm) != 0)
            panic("kvmmap");

        if(a == last)
            break;

        a += PGSIZE;
        pa += PGSIZE;
    }
}

// 映射内核栈的辅助函数
void proc_mapstacks(pagetable_t kpt) {
    struct proc *p;

    for(int i = 0; i < NPROC; i++) {
        p = &proc[i];
        // 1. 分配一个物理页作为栈
        char *pa = alloc_page();
        if(pa == 0) panic("kvminit: kstack alloc failed");

        // 2. 计算这个进程对应的内核栈虚拟地址
        //    使用我们在 memlayout.h 里定义的公式
        uint64 va = VKSTACK(i);

        // 3. 映射到内核页表
        //    注意：我们只映射了一页 (PGSIZE)。如果栈溢出，会碰到下面的 Guard Page (未映射)，触发 Panic。
        kvmmap(kpt, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);

        // 4. 记录在进程结构体中，方便后续使用
        //    注意：这里我们暂时不加锁，因为还在单核启动阶段
        p->kstack = va;
    }
}

// 创建页表结构
// kernel_pagetable 是内核页表的指针
void kvminit(void) {
    printf("kvminit: creating kernel pagetable...\n");
    kernel_pagetable = create_pagetable();

    // 1. 映射 UART
    kvmmap(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);

    // 2. 映射 VIRTIO0 (磁盘，留着以后用)
    kvmmap(kernel_pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

    // 3. 映射 PLIC
    kvmmap(kernel_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

    // 4. 映射 CLINT (通常在这个地址)
    kvmmap(kernel_pagetable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);

    // 5. 映射内核代码段 (.text) - RX
    kvmmap(kernel_pagetable, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

    // 6. 映射内核数据段 + 物理内存 (.data + Free RAM) - RW
    kvmmap(kernel_pagetable, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

    // 7. 映射蹦床 (Trampoline)
    //    这是以后用户态切换的关键。即使现在没用，映射上也无妨。
    //    如果你还没有 trampoline.S，暂时注释掉下面这行，或者映射一个空页。
    // kvmmap(kernel_pagetable, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

    // 8. --- 关键步骤：映射所有进程的内核栈 ---
    printf("kvminit: mapping kernel stacks...\n");
    proc_mapstacks(kernel_pagetable);

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

void handle_page_fault(struct trapframe *tf) {
    uint64 fault_va = r_stval(); // 获取出错地址

    // --- 1. 新的栈溢出检测逻辑 ---
    // 遍历所有可能的内核栈，看看 fault_va 是否落在它们的守护页里
    // 守护页位于：每个 VKSTACK(i) 的下方 (低地址方向)
    int is_stack_overflow = 0;
    for(int i = 0; i < NPROC; i++) {
        uint64 kstack_base = VKSTACK(i);
        // 守护页范围：[栈底 - PGSIZE, 栈底)
        if (fault_va >= (kstack_base - PGSIZE) && fault_va < kstack_base) {
            is_stack_overflow = 1;
            break;
        }
    }

    if (is_stack_overflow) {
        printf("\n!!! PANIC: Kernel Stack Overflow !!!\n");
        printf("Faulting VA: %p (Guard Page Hit)\n", fault_va);
        panic("Kernel Stack Overflow");
    }

    // --- 2. 普通缺页处理 ---
    printf("\n--- Page Fault Occurred! ---\n");
    printf("Faulting Virtual Address: %p\n", fault_va);
    printf("Instruction Pointer (epc): %p\n", tf->epc);

    panic("Page Fault");
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


// ==========================================================
// == 用于响应 sysproc.c 的链接器错误
// ==========================================================

// 验证一个用户虚拟地址的有效性，并返回其物理地址
// 检查: 1. PTE_V (有效) 2. PTE_U (用户可访问)
// 成功返回物理地址，失败返回 0
static uint64
walk_user_addr(pagetable_t pt, uint64 va)
{
    // (你的 walk_lookup 已经检查了 va 上限)
    pte_t *pte = walk_lookup(pt, va);
    if(pte == 0)
        return 0;

    // 关键安全检查：
    if(((*pte & PTE_V) == 0) || ((*pte & PTE_U) == 0))
        return 0;

    return PTE2PA(*pte);
}

// (响应 'copy_from_user')
// 从用户空间安全地拷贝数据到内核空间
int
copy_from_user(void *kernel_dst, uint64 user_src, uint64 len)
{
    char *k_dst = (char*)kernel_dst;
    uint64 va = PGROUNDDOWN(user_src);
    uint64 offset = user_src - va;

    // (你的 proc.h 已被包含，myproc() 可用)
    pagetable_t pt = myproc()->pagetable;

    while(len > 0) {
        uint64 pa = walk_user_addr(pt, va);
        if(pa == 0) {
            return -1; // 无效地址或非用户地址
        }

        uint64 bytes_to_copy = PGSIZE - offset;
        if(bytes_to_copy > len) {
            bytes_to_copy = len;
        }

        // 内核可以直接访问所有物理地址
        memcpy(k_dst, (void*)(pa + offset), bytes_to_copy);

        len -= bytes_to_copy;
        k_dst += bytes_to_copy;
        va += PGSIZE;
        offset = 0; // 只有第一页有 offset
    }
    return 0;
}

// (响应 'copy_to_user')
// 从内核空间安全地拷贝数据到用户空间
int
copy_to_user(uint64 user_dst, void *kernel_src, uint64 len)
{
    char *k_src = (char*)kernel_src;
    uint64 va = PGROUNDDOWN(user_dst);
    uint64 offset = user_dst - va;

    pagetable_t pt = myproc()->pagetable;

    while(len > 0) {
        uint64 pa = walk_user_addr(pt, va);
        if(pa == 0) {
            return -1;
        }

        // 额外检查: 目标地址必须是可写的 (PTE_W)
        pte_t *pte = walk_lookup(pt, va); // 我们知道这会成功
        if((*pte & PTE_W) == 0) {
            return -1; // 目标不可写
        }

        uint64 bytes_to_copy = PGSIZE - offset;
        if(bytes_to_copy > len) {
            bytes_to_copy = len;
        }

        memcpy((void*)(pa + offset), k_src, bytes_to_copy);

        len -= bytes_to_copy;
        k_src += bytes_to_copy;
        va += PGSIZE;
        offset = 0;
    }
    return 0;
}


// (响应 'uvm_copy')
// 为 fork() 复制用户内存
int
uvm_copy(pagetable_t old_pt, pagetable_t new_pt, uint64 sz)
{
    pte_t *pte;
    uint64 pa, i;
    int perm;
    void *new_page_pa;

    // 遍历父进程的整个用户空间
    for(i = 0; i < sz; i += PGSIZE) {
        pte = walk_lookup(old_pt, i);

        // 如果父进程没有映射，或者不是一个有效的用户页，就跳过
        if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) {
            continue;
        }

        pa = PTE2PA(*pte);
        perm = (*pte) & 0x3FF; // 复制所有权限位 (V,R,W,X,U,...)

        // 1. 分配一页新的物理内存 (来自 pmm.h)
        new_page_pa = alloc_page();
        if(new_page_pa == 0) {
            goto err; // 内存不足
        }

        // 2. 将父进程的物理页内容复制到新页
        memcpy(new_page_pa, (void*)pa, PGSIZE);

        // 3. 将新页映射到子进程的页表中
        if(map_page(new_pt, i, (uint64)new_page_pa, perm) != 0) {
            free_page(new_page_pa); // (来自 pmm.h)
            goto err;
        }
    }
    return 0;

err:
    // 错误处理：如果中途失败，需要取消映射并释放所有已分配的页
    // (这是一个简化的错误处理，更健壮的实现会调用 free_walk)
    destroy_pagetable(new_pt); // 销毁不完整的子页表
    printf("uvm_copy: failed\n");
    return -1;
}