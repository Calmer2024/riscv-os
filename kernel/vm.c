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
// 声明外部定义的蹦床代码
extern char trampoline[];

// kernel_pagetable 是内核页表的指针
pagetable_t kernel_pagetable;

// 内部辅助函数声明
pte_t* walk(pagetable_t pagetable, uint64 va, int alloc);
void   uvmfree(pagetable_t pagetable, uint64 sz);
pte_t* walk_create(pagetable_t pt, uint64 va);
void free_walk(pagetable_t pt);
void dump_walk(pagetable_t pt, int level, uint64 current_va);

// ====================================================================
// Core Mapping Logic (核心映射逻辑)
// ====================================================================

pte_t*
walk(pagetable_t pagetable, uint64 va, int alloc)
{
    if(va >= MAXVA)
        panic("walk");

    for(int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if(*pte & PTE_V) {
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {
            if(!alloc || (pagetable = (pagetable_t)alloc_page()) == 0)
                return 0;
            memset(pagetable, 0, PAGE_SIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}

// 查找虚拟地址 va 对应的物理地址 (仅用于用户地址)
// 如果未映射或权限不对，返回 0
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
    pte_t *pte;
    uint64 pa;

    if(va >= MAXVA)
        return 0;

    pte = walk(pagetable, va, 0);
    if(pte == 0)
        return 0;
    if((*pte & PTE_V) == 0)
        return 0;
    if((*pte & PTE_U) == 0) // 必须是用户页
        return 0;

    pa = PTE2PA(*pte);
    return pa;
}

// [通用映射函数]
// 将虚拟地址范围 [va, va+size] 映射到物理地址 pa
// 这是一个核心函数，替换了你之前的 kvmmap 和 map_page
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
    uint64 a, last;
    pte_t *pte;

    if(size == 0)
        panic("mappages: size");

    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);

    for(;;){
        if((pte = walk(pagetable, a, 1)) == 0)
            return -1;
        if(*pte & PTE_V)
            panic("mappages: remap");

        *pte = PA2PTE(pa) | perm | PTE_V;

        if(a == last)
            break;
        a += PAGE_SIZE;
        pa += PAGE_SIZE;
    }
    return 0;
}

// void kvmmap(pagetable_t kpt, uint64 va, uint64 pa, uint64 sz, int perm) {
//     uint64 a = PGROUNDDOWN(va);
//     uint64 last = PGROUNDDOWN(va + sz - 1);
//
//     for(;;){
//         if(map_page(kpt, a, pa, perm) != 0)
//             panic("kvmmap");
//
//         if(a == last)
//             break;
//
//         a += PAGE_SIZE;
//         pa += PAGE_SIZE;
//     }
// }

// ====================================================================
// Kernel VM Initialization (内核虚存初始化)
// ====================================================================

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
        //    注意：我们只映射了一页 (PAGE_SIZE)。如果栈溢出，会碰到下面的 Guard Page (未映射)，触发 Panic。
        // kvmmap(kpt, va, (uint64)pa, PAGE_SIZE, PTE_R | PTE_W);
        mappages(kpt, va, PAGE_SIZE, (uint64)pa, PTE_R | PTE_W);

        // 4. 记录在进程结构体中，方便后续使用
        //    注意：这里我们暂时不加锁，因为还在单核启动阶段
        p->kstack = va;
    }
}

// 创建内核页表
pagetable_t kvmmake(void) {
    pagetable_t kpt = (pagetable_t)alloc_page();
    memset(kpt, 0, PAGE_SIZE);

    // 1. UART
    mappages(kpt, UART0, PAGE_SIZE, UART0, PTE_R | PTE_W);
    // 2. VIRTIO0
    mappages(kpt, VIRTIO0, PAGE_SIZE, VIRTIO0, PTE_R | PTE_W);
    // 3. PLIC
    mappages(kpt, PLIC, 0x400000, PLIC, PTE_R | PTE_W);
    // 4. CLINT
    mappages(kpt, CLINT, 0x10000, CLINT, PTE_R | PTE_W);
    // 5. Kernel Text (R-X)
    mappages(kpt, KERNBASE, (uint64)etext - KERNBASE, KERNBASE, PTE_R | PTE_X);
    // 6. Kernel Data + RAM (RW-)
    mappages(kpt, (uint64)etext, PHYSTOP - (uint64)etext, (uint64)etext, PTE_R | PTE_W);
    // 7. Trampoline (RX) - [关键] 内核页表也必须映射蹦床
    mappages(kpt, TRAMPOLINE, PAGE_SIZE, (uint64)trampoline, PTE_R | PTE_X);

    return kpt;
}

void kvminit(void) {
    printf("kvminit: creating kernel pagetable...\n");
    kernel_pagetable = kvmmake();

    // 映射所有进程的内核栈
    printf("kvminit: mapping kernel stacks...\n");
    proc_mapstacks(kernel_pagetable);
}

void kvminithart(void) {
    printf("kvminithart: enabling paging...\n");
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();
    printf("kvminithart: paging enabled.\n");
}

// ====================================================================
// User VM Management (用户虚拟内存管理)
// ====================================================================
// 创建一个空的用户页表
pagetable_t uvmcreate() {
    pagetable_t pagetable;
    pagetable = (pagetable_t)alloc_page();
    if(pagetable == 0)
        return 0;
    memset(pagetable, 0, PAGE_SIZE);
    return pagetable;
}

// 加载 initcode 到用户页表的起始位置 (用于第一个进程)
// src: initcode 的二进制代码, sz: 大小
void uvminit(pagetable_t pagetable, uchar *src, uint sz) {
    char *mem;

    if(sz >= PAGE_SIZE)
        panic("inituvm: more than a page");

    mem = alloc_page();
    memset(mem, 0, PAGE_SIZE);

    // 映射地址 0 到 物理地址 mem
    mappages(pagetable, 0, PAGE_SIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);

    // 拷贝代码
    memmove(mem, src, sz);
}

// 解除映射
// va: 虚拟地址 (必须页对齐)
// npages: 页数
// do_free: 是否释放对应的物理页
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free) {
    uint64 a;
    pte_t *pte;

    if((va % PAGE_SIZE) != 0)
        panic("uvmunmap: not aligned");

    for(a = va; a < va + npages*PAGE_SIZE; a += PAGE_SIZE){
        if((pte = walk(pagetable, a, 0)) == 0)
            panic("uvmunmap: walk");

        if((*pte & PTE_V) == 0)
            panic("uvmunmap: not mapped");

        if(PTE_FLAGS(*pte) == PTE_V)
            panic("uvmunmap: not a leaf");

        if(do_free){
            uint64 pa = PTE2PA(*pte);
            free_page((void*)pa);
        }
        *pte = 0; // 清除 PTE
    }
}

// 释放用户内存 (解除映射)
// 将大小从 oldsz 减少到 newsz
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
    if(newsz >= oldsz)
        return oldsz;

    if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
        int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PAGE_SIZE;
        uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
    }

    return newsz;
}

// 分配用户内存 (用于 sbrk 或 exec)
// oldsz: 旧的大小, newsz: 新的大小
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm) {
    char *mem;
    uint64 a;

    if(newsz < oldsz)
        return oldsz;

    oldsz = PGROUNDUP(oldsz);
    for(a = oldsz; a < newsz; a += PAGE_SIZE){
        mem = alloc_page();
        if(mem == 0){
            uvmdealloc(pagetable, a, oldsz); // 失败时回滚
            return 0;
        }
        memset(mem, 0, PAGE_SIZE);
        if(mappages(pagetable, a, PAGE_SIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
            free_page(mem);
            uvmdealloc(pagetable, a, oldsz);
            return 0;
        }
    }
    return newsz;
}

// 递归释放页表页面 (但不释放叶子节点指向的物理页，那是 uvmunmap 的工作)
void freewalk(pagetable_t pagetable) {
    for(int i = 0; i < 512; i++){
        pte_t pte = pagetable[i];
        if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
            // 是中间节点
            uint64 child = PTE2PA(pte);
            freewalk((pagetable_t)child);
            pagetable[i] = 0;
        } else if(pte & PTE_V){
            // 是叶子节点，这里不应该发生，因为 uvmfree 应该先 unmap 了
            // 但如果有的页表只被部分 unmap，这里就会Panic
            // panic("freewalk: leaf");
        }
    }
    free_page((void*)pagetable);
}

// 释放整个用户进程空间
// 1. 解除映射并释放物理内存
// 2. 释放页表本身
void uvmfree(pagetable_t pagetable, uint64 sz) {
    if(sz > 0)
        uvmunmap(pagetable, 0, PGROUNDUP(sz)/PAGE_SIZE, 1);
    freewalk(pagetable);
}

// 复制父进程内存到子进程 (Fork)
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz) {
    pte_t *pte;
    uint64 pa, i;
    uint flags;
    char *mem;

    for(i = 0; i < sz; i += PAGE_SIZE){
        if((pte = walk(old, i, 0)) == 0)
            panic("uvmcopy: pte should exist");
        if((*pte & PTE_V) == 0)
            panic("uvmcopy: page not present");

        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);

        if((mem = alloc_page()) == 0)
            goto err;

        memmove(mem, (char*)pa, PAGE_SIZE);

        if(mappages(new, i, PAGE_SIZE, (uint64)mem, flags) != 0){
            free_page(mem);
            goto err;
        }
    }
    return 0;

 err:
    uvmunmap(new, 0, i / PAGE_SIZE, 1);
    return -1;
}

// ====================================================================
// Data Copy Helpers (内核 <-> 用户 数据拷贝)
// ====================================================================
// 从用户空间拷贝数据到内核
// 替代了之前的 copy_from_user
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) {
    uint64 n, va0, pa0;

    while(len > 0){
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if(pa0 == 0)
            return -1;

        n = PAGE_SIZE - (srcva - va0);
        if(n > len)
            n = len;

        memmove(dst, (void *)(pa0 + (srcva - va0)), n);

        len -= n;
        dst += n;
        srcva = va0 + PAGE_SIZE;
    }
    return 0;
}

// 从内核拷贝数据到用户空间
// 替代了之前的 copy_to_user
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len) {
    uint64 n, va0, pa0;

    while(len > 0){
        va0 = PGROUNDDOWN(dstva);
        pa0 = walkaddr(pagetable, va0);
        if(pa0 == 0)
            return -1;

        n = PAGE_SIZE - (dstva - va0);
        if(n > len)
            n = len;

        memmove((void *)(pa0 + (dstva - va0)), src, n);

        len -= n;
        src += n;
        dstva = va0 + PAGE_SIZE;
    }
    return 0;
}

// 为了兼容你之前的命名，做一层封装 (可选)
int copy_from_user(void *dst, uint64 src, uint64 len) {
    return copyin(myproc()->pagetable, (char*)dst, src, len);
}

int copy_to_user(uint64 dst, void *src, uint64 len) {
    return copyout(myproc()->pagetable, dst, (char*)src, len);
}

// 从用户虚拟地址空间中，安全地拷贝一个以 \0 结尾的字符串到内核缓冲区
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
    uint64 n, va0, pa0;
    int got_null = 0;

    while(got_null == 0 && max > 0){
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if(pa0 == 0)
            return -1;
        n = PAGE_SIZE - (srcva - va0);
        if(n > max)
            n = max;

        char *p = (char *) (pa0 + (srcva - va0));
        while(n > 0){
            if(*p == '\0'){
                *dst = '\0';
                got_null = 1;
                break;
            } else {
                *dst = *p;
            }
            --n;
            --max;
            p++;
            dst++;
        }

        srcva = va0 + PAGE_SIZE;
    }
    if(got_null){
        return 0;
    } else {
        return -1;
    }
}

// ====================================================================
// Fault Handler (异常处理)
// ====================================================================
void handle_page_fault(struct trapframe *tf) {
    uint64 fault_va = r_stval(); // 获取出错地址

    // --- 1. 新的栈溢出检测逻辑 ---
    // 遍历所有可能的内核栈，看看 fault_va 是否落在它们的守护页里
    // 守护页位于：每个 VKSTACK(i) 的下方 (低地址方向)
    int is_stack_overflow = 0;
    for(int i = 0; i < NPROC; i++) {
        uint64 kstack_base = VKSTACK(i);
        // 守护页范围：[栈底 - PAGE_SIZE, 栈底)
        if (fault_va >= (kstack_base - PAGE_SIZE) && fault_va < kstack_base) {
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

/* 下面均为辅助函数 */
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

// 销毁一个完整的页表
void destroy_pagetable(pagetable_t pt) {
    if (pt == NULL) return;
    free_walk(pt);
}


//
// // 页表管理工具
// // 分配一个物理页作为根页表
// pagetable_t create_pagetable(void) {
//     return (pagetable_t)alloc_page();
// }
//
// // 将一个虚拟页映射到一个物理页
// int map_page(pagetable_t pt, uint64 va, uint64 pa, int perm) {
//     if (va % PAGE_SIZE != 0 || pa % PAGE_SIZE != 0) {
//         panic("map_page: unaligned addresses");
//     }
//
//     pte_t *pte = walk_create(pt, va);
//     if (pte == 0) return -1;
//
//     if (*pte & PTE_V) {
//         panic("map_page: remap");
//     }
//
//     *pte = PA2PTE(pa) | perm | PTE_V;
//     return 0;
// }
//
//
//
// // 查找现有映射，如果有效就返回地址，无效就返回0
// pte_t* walk_lookup(pagetable_t pt, uint64 va) {
//     if(va >= (1L << 39)) {
//         return 0;
//     }
//
//     for (int level = 2; level > 0; level--) {
//         pte_t *pte = &pt[PX(level, va)];
//         if (*pte & PTE_V) {
//             // PTE有效，获取下一级页表的物理地址
//             pt = (pagetable_t)PTE2PA(*pte);
//         } else {
//             // PTE无效，意味着映射不存在，查找失败
//             return 0;
//         }
//     }
//     // 返回最末级（L0）PTE的地址
//     return &pt[PX(0, va)];
// }
//
// // 查找现有映射，如果有效就返回地址，无效就创建一个新的页进行映射
// pte_t* walk_create(pagetable_t pt, uint64 va) {
//     if(va >= (1L << 39)) panic("walk_create: va too large");
//
//     for (int level = 2; level > 0; level--) {
//         pte_t *pte = &pt[PX(level, va)];
//         if (*pte & PTE_V) {
//             pt = (pagetable_t)PTE2PA(*pte);
//         } else {
//             pt = (pagetable_t)alloc_page();
//             if (pt == 0) return 0;
//             memset(pt, 0, PAGE_SIZE);
//             *pte = PA2PTE(pt) | PTE_V;
//         }
//     }
//     return &pt[PX(0, va)];
// }
//

//
//
//
// void dump_pagetable(pagetable_t pt) {
//     printf("--- Pagetable Dump (Root at PA: 0x%lx) ---\n", (uint64)pt);
//     dump_walk(pt, 2, 0);
//     printf("--- End of Dump ---\n");
// }
//
//
// // ==========================================================
// // == 用于响应 sysproc.c 的链接器错误
// // ==========================================================
//
// // 验证一个用户虚拟地址的有效性，并返回其物理地址
// // 检查: 1. PTE_V (有效) 2. PTE_U (用户可访问)
// // 成功返回物理地址，失败返回 0
// static uint64
// walk_user_addr(pagetable_t pt, uint64 va)
// {
//     // (你的 walk_lookup 已经检查了 va 上限)
//     pte_t *pte = walk_lookup(pt, va);
//     if(pte == 0)
//         return 0;
//
//     // 关键安全检查：
//     if(((*pte & PTE_V) == 0) || ((*pte & PTE_U) == 0))
//         return 0;
//
//     return PTE2PA(*pte);
// }
//
// // (响应 'copy_from_user')
// // 从用户空间安全地拷贝数据到内核空间
// int
// copy_from_user(void *kernel_dst, uint64 user_src, uint64 len)
// {
//     char *k_dst = (char*)kernel_dst;
//     uint64 va = PGROUNDDOWN(user_src);
//     uint64 offset = user_src - va;
//
//     // (你的 proc.h 已被包含，myproc() 可用)
//     pagetable_t pt = myproc()->pagetable;
//
//     while(len > 0) {
//         uint64 pa = walk_user_addr(pt, va);
//         if(pa == 0) {
//             return -1; // 无效地址或非用户地址
//         }
//
//         uint64 bytes_to_copy = PAGE_SIZE - offset;
//         if(bytes_to_copy > len) {
//             bytes_to_copy = len;
//         }
//
//         // 内核可以直接访问所有物理地址
//         memcpy(k_dst, (void*)(pa + offset), bytes_to_copy);
//
//         len -= bytes_to_copy;
//         k_dst += bytes_to_copy;
//         va += PAGE_SIZE;
//         offset = 0; // 只有第一页有 offset
//     }
//     return 0;
// }
//
// // (响应 'copy_to_user')
// // 从内核空间安全地拷贝数据到用户空间
// int
// copy_to_user(uint64 user_dst, void *kernel_src, uint64 len)
// {
//     char *k_src = (char*)kernel_src;
//     uint64 va = PGROUNDDOWN(user_dst);
//     uint64 offset = user_dst - va;
//
//     pagetable_t pt = myproc()->pagetable;
//
//     while(len > 0) {
//         uint64 pa = walk_user_addr(pt, va);
//         if(pa == 0) {
//             return -1;
//         }
//
//         // 额外检查: 目标地址必须是可写的 (PTE_W)
//         pte_t *pte = walk_lookup(pt, va); // 我们知道这会成功
//         if((*pte & PTE_W) == 0) {
//             return -1; // 目标不可写
//         }
//
//         uint64 bytes_to_copy = PAGE_SIZE - offset;
//         if(bytes_to_copy > len) {
//             bytes_to_copy = len;
//         }
//
//         memcpy((void*)(pa + offset), k_src, bytes_to_copy);
//
//         len -= bytes_to_copy;
//         k_src += bytes_to_copy;
//         va += PAGE_SIZE;
//         offset = 0;
//     }
//     return 0;
// }
//
//
// // (响应 'uvm_copy')
// // 为 fork() 复制用户内存
// int
// uvm_copy(pagetable_t old_pt, pagetable_t new_pt, uint64 sz)
// {
//     pte_t *pte;
//     uint64 pa, i;
//     int perm;
//     void *new_page_pa;
//
//     // 遍历父进程的整个用户空间
//     for(i = 0; i < sz; i += PAGE_SIZE) {
//         pte = walk_lookup(old_pt, i);
//
//         // 如果父进程没有映射，或者不是一个有效的用户页，就跳过
//         if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) {
//             continue;
//         }
//
//         pa = PTE2PA(*pte);
//         perm = (*pte) & 0x3FF; // 复制所有权限位 (V,R,W,X,U,...)
//
//         // 1. 分配一页新的物理内存 (来自 pmm.h)
//         new_page_pa = alloc_page();
//         if(new_page_pa == 0) {
//             goto err; // 内存不足
//         }
//
//         // 2. 将父进程的物理页内容复制到新页
//         memcpy(new_page_pa, (void*)pa, PAGE_SIZE);
//
//         // 3. 将新页映射到子进程的页表中
//         if(map_page(new_pt, i, (uint64)new_page_pa, perm) != 0) {
//             free_page(new_page_pa); // (来自 pmm.h)
//             goto err;
//         }
//     }
//     return 0;
//
// err:
//     // 错误处理：如果中途失败，需要取消映射并释放所有已分配的页
//     // (这是一个简化的错误处理，更健壮的实现会调用 free_walk)
//     destroy_pagetable(new_pt); // 销毁不完整的子页表
//     printf("uvm_copy: failed\n");
//     return -1;
// }