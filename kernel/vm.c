#include "../include/types.h"
#include "../include/riscv.h"
#include "../include/memlayout.h"
#include "../include/kalloc.h"
#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/string.h"

// 内核根页表
pagetable_t kernel_root_pagetable;

// 内核代码段结束地址
extern char etext[];
extern char ptrampoline[]; // trampoline.S


// 创建一个页表，然后填充0，返回该页表的首地址
// 如果内存耗尽返回0（目前kmem_alloc会panic）
pagetable_t vmem_create_pagetable(void) {
    // 分配一个物理页
    pagetable_t pagetable = kmem_alloc();
    if (!pagetable) return 0; // 物理页分配失败
    memset(pagetable, 0, PAGE_SIZE);
    return pagetable;
}

// 根据虚拟地址 virtual_addr 找到对应的页表项，返回页表项的地址
// 如果alloc非0，则分配物理内存，并设置页表项
// 仿照xv6的写法，在查找的过程中根据alloc决定是否创建
// 如果没找到，返回0
pte_t *vmem_walk_pte(pagetable_t pagetable, uint64 virtual_addr, int alloc) {
    if (virtual_addr >= MAX_VIRTUAL_ADDR) {
        panic("vmem_walk: too large virtual address");
    }
    // 逐层获取
    for (int level = 2; level > 0; level--) {
        int index = PPN(virtual_addr, level); // k级页表内的索引
        pte_t *pte = pagetable + index; // 获取页表项的地址
        if (*pte & PTE_V) {
            // 该页有效，已经存在
            pagetable = (pagetable_t) PTE_TO_PA(*pte);
        } else {
            if (!alloc) return 0; // 不分配，直接失败
            pagetable = vmem_create_pagetable(); // 分配一个物理页
            if (!pagetable) return 0; // 物理页分配失败，目前 kmem_alloc 会直接panic
            // 回到上一级的PTE（*pte）把刚申请到的新页表的物理地址填进去，
            // 并把它标记为有效。（只会标记三级和二级的，一级的不会标记）
            *pte = PA_TO_PTE(pagetable) | PTE_V;
        }
    }
    // 返回最终的、第三级PTE的地址
    // 循环结束后，pagetable 变量已经指向了最底层的、第三级的页表。
    // 最终返回这个PTE的地址，让调用者去填写。
    int index = PPN(virtual_addr, 0);
    return pagetable + index;
}

// 映射一个页表（va->pa）
// permission：权限位，对应PTE的R,W,X,U,G,A,D,RWXUGA，一般用到RWX
// TODO:这个操作不是原子的，分配失败没有释放
int vmem_map_pagetable(pagetable_t pagetable, uint64 virtual_addr, uint64 physical_addr, int permission) {
    if (virtual_addr % PAGE_SIZE != 0) {
        panic("vmem_map_pagetable: virtual_addr not aligned");
    }
    // 物理地址要对齐吗？不对齐会不会混乱？好像不会，PA_TO_PTE会自动对齐，但是感觉对齐还是比较好
    if (physical_addr % PAGE_SIZE != 0) {
        panic("vmem_map_pagetable: physical_addr not aligned");
    }
    pte_t *pte = vmem_walk_pte(pagetable, virtual_addr, 1);
    if (pte == 0) return -1; // 创建页表项失败
    if (*pte & PTE_V) {
        // 该页已经存在并且被映射过
        panic("vmem_map_pagetable: remap pagetable");
    }
    *pte = PA_TO_PTE(physical_addr) | (permission & 0x1fe) | PTE_V;
    return 0;
}

// 解除映射，如果do_free为真，则同时释放物理页，va必须对齐
int vmem_unmap_pagetable(pagetable_t pagetable, uint64 virtual_addr, int do_free) {
    if (virtual_addr % PAGE_SIZE != 0) {
        panic("vmem_unmap_pagetable: virtual_addr not aligned");
    }
    // 1. 查找PTE，但不创建
    pte_t *pte = vmem_walk_pte(pagetable, virtual_addr, 0);
    // 2. 检查是否真的映射了
    if (pte == 0) {
        return -1; // 没有找到页表项 (可能是L1或L2目录不存在)
    }
    if ((*pte & PTE_V) == 0) {
        return -1; // 该页未映射
    }

    // 释放物理内存
    if (do_free) {
        uint64 pa = PTE_TO_PA(*pte);
        kmem_free((void*)pa);
    }

    // 5. 将PTE清零，使其无效
    *pte = 0;

    return 0; // 成功
}


// 递归地释放一个页表，如果非叶子节点，先释放下一级的节点
// 这个函数只负责释放页表所占的内存，不会释放物理页，
// 释放前要保证物理页的映射已经被移除了，否则会panic
void vmem_free_pagetable(pagetable_t pagetable) {
    for (int i = 0; i < LEAF_PTES; i++) {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
            // 这个pte有效并且不是叶子节点
            uint64 child = PTE_TO_PA(pte);
            // 递归清除子节点
            vmem_free_pagetable((pagetable_t) child);
            // 子节点清除完毕，清除自己
            pagetable[i] = 0;
        } else if (pte & PTE_V) {
            // 这个pte有效但并且是叶子节点
            panic("vmem_free_pagetable: leaf PTE found");
        }
    }
    // 释放自身
    kmem_free(pagetable);
}

// kernel/vm.c

// 调试函数：打印单层页表的内容
// pagetable: 要打印的页表的物理地址
// level: 当前页表的级别 (2 for L1, 1 for L2, 0 for L3)
void vmem_pagetable_dump(pagetable_t pagetable, int level) {
    // 打印表头
    printf_color("=== Page Table (Level %d) at PA: %p ===\n", PURPLE, level, pagetable);

    // 遍历 512 个 PTE
    for (int i = 0; i < LEAF_PTES; i++) {
        pte_t *pte = pagetable + i;

        // 只打印有效的PTE，忽略无效的
        if (*pte & PTE_V) {
            uint64 child_pa = PTE_TO_PA(*pte);
            printf("  PTE[%d]: %p ", i, pte);

            // 判断是叶子节点还是中间节点
            if ((*pte & (PTE_R | PTE_W | PTE_X)) == 0) {
                // 中间节点：V=1, 但 R,W,X=0
                printf("-> Points to Next Level PT @ PA: %p\n", (void *) child_pa);
            } else {
                // 叶子节点：V=1, 且 R,W,X 中至少有一个不为0
                printf("-> Maps to PA: %p | Perms: ", (void *) child_pa);
                printf("%c", (*pte & PTE_R) ? 'R' : '-');
                printf("%c", (*pte & PTE_W) ? 'W' : '-');
                printf("%c", (*pte & PTE_X) ? 'X' : '-');
                printf("%c", (*pte & PTE_U) ? 'U' : '-');
                printf("\n");
            }
        }
    }
    printf("=== End of Page Table Dump ===\n");
}

void vmem_init(void) {
    // 分配一个内核根页表
    kernel_root_pagetable = vmem_create_pagetable();
    if (!kernel_root_pagetable) {
        panic("vmem_init: kernel root pagetable alloc failed");
    }
    // 映射VARTIO
    vmem_map_pagetable(kernel_root_pagetable, VIRTIO0, VIRTIO0, PTE_R | PTE_W);

    // 映射UART串口
    vmem_map_pagetable(kernel_root_pagetable, UART, UART, PTE_R | PTE_W);
    vmem_map_pagetable(kernel_root_pagetable, QEMU_POWEROFF_ADDR, QEMU_POWEROFF_ADDR, PTE_R | PTE_W);
    // 映射PLIC
    for (uint64 va = PLIC, pa = PLIC; pa < (uint64) (PLIC + 0x4000000); va += PAGE_SIZE, pa += PAGE_SIZE) {
        vmem_map_pagetable(kernel_root_pagetable, va, pa,PTE_R | PTE_W);
    }
    // 映射代码段
    for (uint64 va = KERNEL_BASE, pa = KERNEL_BASE; pa < (uint64) etext; va += PAGE_SIZE, pa += PAGE_SIZE) {
        vmem_map_pagetable(kernel_root_pagetable, va, pa,PTE_R | PTE_X);
    }
    // 映射内核数据与剩余物理内存
    for (uint64 va = PAGE_UP((uint64)etext), pa = PAGE_UP((uint64)etext);
         pa < PHYS_TOP;
         va += PAGE_SIZE, pa += PAGE_SIZE) {
        vmem_map_pagetable(kernel_root_pagetable, va, pa,PTE_R | PTE_W);
    }
    // 映射跳板页面，内核栈在分配进程的时候进行映射
    vmem_map_pagetable(kernel_root_pagetable,TRAMPOLINE, (uint64) ptrampoline,PTE_R | PTE_X);
    printf_color("vmem_init: kernal pagetable created.\n",BLACK);
}

// 复制一个进程的页表给另一个，用于fork
int vmem_user_copy(pagetable_t src_pt, pagetable_t dst_pt, uint64 size) {
    uint64 va;
    pte_t *pte;

    // 循环遍历父进程 [0, size) 范围内的所有虚拟页
    for (va = 0; va < size; va += PAGE_SIZE) {
        // 1. 查找父进程的 PTE
        pte = vmem_walk_pte(src_pt, va, 0); // alloc=0, 不创建
        if (pte == 0 || (*pte & PTE_V) == 0) {
            continue; // 父进程没有映射这页，跳过
        }

        // 2. 分配一页新的物理内存给子进程
        char *new_pa = kmem_alloc();
        if (new_pa == 0) {
            // (这里应该有一个错误回滚，释放所有已分配的页)
            return -1; // 内存不足
        }

        // 3. 复制父进程的物理页内容到子进程的新物理页
        uint64 src_pa = PTE_TO_PA(*pte);
        memmove(new_pa, (void*)src_pa, PAGE_SIZE);

        // 4. 将子进程的新物理页映射到 *相同* 的虚拟地址
        int flags = PTE_FLAGS(*pte); // 获取旧的权限 (R,W,X,U)
        if (vmem_map_pagetable(dst_pt, va, (uint64)new_pa, flags) != 0) {
            kmem_free(new_pa);
            // (错误回滚)
            return -1;
        }
    }
    return 0; // 成功
}

// 复制栈，用于fork
int vmem_stack_copy(pagetable_t src_pt, pagetable_t dst_pt) {
    // 目前栈只有一页
    pte_t *pte = vmem_walk_pte(src_pt, USER_STACK_VA, 0);

    if (pte == 0 || (*pte & PTE_V) == 0) {
        panic("fork: no stack found"); // 或者返回-1
    }

    char *new_pa = kmem_alloc();
    if (new_pa == 0) {
        return -1;
    }

    uint64 src_pa = PTE_TO_PA(*pte);
    memmove(new_pa, (void*)src_pa, PAGE_SIZE);

    int flags = PTE_FLAGS(*pte);
    if (vmem_map_pagetable(dst_pt, USER_STACK_VA, (uint64)new_pa, flags) != 0) {
        kmem_free(new_pa);
        return -1;
    }

    return 0;
}

// 安全地从用户空间复制数据到内核空间,
int vmem_copyin(pagetable_t pagetable, char *dst_kernel, uint64 src_user, uint64 len) {
    uint64 n, va_start, pa_base;
    pte_t *pte;

    while (len > 0) {
        // 1. 检查虚拟地址是否在用户空间范围内
        if (src_user >= MAX_USER_VA) {
            return -1;
        }
        // 2. 找到该虚拟地址所在的页的PTE
        pte = vmem_walk_pte(pagetable, src_user, 0);
        // 3. 安全检查
        //    PTE 必须存在 (pte != 0)
        //    PTE 必须是有效的 (PTE_V)
        //    PTE 必须是用户可访问的 (PTE_U)
        if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0) {
            return -1; // 非法地址！
        }
        // 4. 计算可以从当前这一个物理页复制多少字节
        va_start = PAGE_DOWN(src_user); // 当前页的起始虚拟地址
        pa_base = PTE_TO_PA(*pte);      // 当前页的起始物理地址

        n = PAGE_SIZE - (src_user - va_start); // 当前页还剩多少字节
        if (n > len) {
            n = len; // 我们只需要 len 这么多
        }

        // 5. 复制数据
        //    (pa_base + (src_user - va_start)) 是 src_user 对应的物理地址
        memmove(dst_kernel, (void *)(pa_base + (src_user - va_start)), n);

        // 6. 更新循环变量
        len -= n;
        dst_kernel += n;
        src_user += n;
    }

    return 0; // 成功
}

// 获取虚拟地址对应的物理地址
uint64 vmem_walk_addr(pagetable_t pagetable, uint64 va)
{
    pte_t *pte;
    uint64 pa;

    if(va >= MAX_VIRTUAL_ADDR)
        return 0;

    pte = vmem_walk_pte(pagetable, va, 0);
    if(pte == 0)
        return 0;
    if((*pte & PTE_V) == 0)
        return 0;
    if((*pte & PTE_U) == 0)
        return 0;
    pa = PTE_TO_PA(*pte);
    return pa;
}

// 安全地从内核空间复制数据到用户空间
int vmem_copyout(pagetable_t pagetable, uint64 dst_user, char *src_kernel, uint64 len) {
    uint64 n, va_start, pa_base;
    pte_t *pte;

    while (len > 0) {
        // 1. 检查虚拟地址
        if (dst_user >= MAX_USER_VA) {
            return -1;
        }

        // 2. 找到PTE
        pte = vmem_walk_pte(pagetable, dst_user, 0);

        // 3. 安全检查
        //    除了 V 和 U，我们必须检查可写权限 W
        if (pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 || (*pte & PTE_W) == 0) {
            return -1; // 非法地址或不可写！
        }

        // 4. 计算可以往当前这一个物理页写多少字节
        va_start = PAGE_DOWN(dst_user);
        pa_base = PTE_TO_PA(*pte);

        n = PAGE_SIZE - (dst_user - va_start);
        if (n > len) {
            n = len;
        }

        // 5. 复制数据 (方向相反)
        memmove((void *)(pa_base + (dst_user - va_start)), src_kernel, n);

        // 6. 更新循环变量
        len -= n;
        src_kernel += n;
        dst_user += n;
    }

    return 0; // 成功
}

int vmem_user_alloc(pagetable_t pagetable, uint64 old_size, uint64 new_size) {
    if(new_size < old_size)
        return 0;
    uint64 va = PAGE_UP(old_size);
    // 循环直到覆盖了 new_size
    for (; va < new_size; va += PAGE_SIZE) {
        char *pa = kmem_alloc(); // 分配一页物理内存
        if (pa == 0) {
            // TODO: 这里应该有一个回滚，释放所有已分配的页
            return -1; // 内存耗尽
        }
        memset(pa, 0, PAGE_SIZE); // 清零

        if (vmem_map_pagetable(pagetable, va, (uint64)pa, PTE_U | PTE_R | PTE_W) != 0) {
            kmem_free(pa);
            // 回滚
            return -1;
        }
    }
    return 0; // 成功
}

uint64 vmem_user_dealloc(pagetable_t pagetable, uint64 old_size, uint64 new_size) {
    if(new_size >= old_size)
        return old_size;
    uint64 va = PAGE_UP(new_size);
    for (;va < old_size; va += PAGE_SIZE) {
        vmem_unmap_pagetable(pagetable, va, 1);
    }
    return new_size;
}

// 测试建立映射函数正确性
void test_vmem_mapping(void) {
    printf("\n");
    printf_color("=== Running VM mapping test ===\n", BLUE);

    // 1. 创建一个临时的根页表
    pagetable_t test_pt = vmem_create_pagetable();
    if (!test_pt) {
        panic("test_vmem_mapping: failed to create pagetable");
    }
    printf("Test pagetable created at PA: %p\n", test_pt);

    // 2. 准备要映射的地址
    // 0b010010001,100100001,000000000000, PTE[0] -> PTE[145] -> PTE[289]
    uint64 va = 0x12321000; // 挑选一个简单的、页对齐的虚拟地址
    uint64 pa = (uint64) kmem_alloc(); // 动态申请一个物理页
    if (pa == 0) {
        panic("test_vmem_mapping: kmem_alloc failed");
    }
    printf("Mapping VA %p -> PA %p\n", (void *) va, (void *) pa);

    // 3. 执行映射
    int result = vmem_map_pagetable(test_pt, va, pa, PTE_R | PTE_W);
    if (result != 0) {
        panic("test_vmem_mapping: map failed");
    }
    printf("Mapping successful.\n");

    // 4. 验证映射
    printf("Verifying mapping...\n");
    pte_t *pte = vmem_walk_pte(test_pt, va, 0); // 使用 alloc=0 来查找

    if (pte == 0) {
        panic("test_vmem_mapping: walk failed to find PTE");
    }
    if ((*pte & PTE_V) == 0) {
        panic("test_vmem_mapping: PTE not valid");
    }
    if (PTE_TO_PA(*pte) != pa) {
        panic("test_vmem_mapping: PA in PTE is incorrect");
    }
    printf("VA %p -> PA %p\n", (void *) va, (void *) PTE_TO_PA(*pte));
    printf("Verification successful: PTE content is correct.\n");

    vmem_pagetable_dump(test_pt, 2);
    vmem_pagetable_dump((pagetable_t) 0x0000000087ffd000, 1);
    vmem_pagetable_dump((pagetable_t) 0x0000000087ffc000, 0);

    // // 这里应该panic，说该物理页没释放
    // vmem_free_pagetable(test_pt);
    // kmem_free((void *) pa);

    kmem_free((void *) pa);
    *pte = 0;
    vmem_free_pagetable(test_pt);
    printf("Pagetable free successful\n");

    printf_color("=== VM mapping test PASSED ===\n", GREEN);
}

// 测试 vmem_init 创建的内核页表是否正确
void test_kernel_pagetable(void) {
    printf_color("=== Running kernel pagetable test ===\n",BLUE);

    pte_t *pte;
    uint64 pa;

    // 0. 检查内核根页表
    if (!kernel_root_pagetable) panic("test_kernal_pagetable: kernel root pagetable not valid");

    // 1. 检查 UART 的映射
    pte = vmem_walk_pte(kernel_root_pagetable, UART, 0);
    if (pte == 0) panic("test_kernel_pagetable: UART not mapped");
    if ((*pte & PTE_V) == 0) panic("test_kernel_pagetable: UART PTE not valid");
    pa = PTE_TO_PA(*pte);
    if (pa != UART) panic("test_kernel_pagetable: UART wrong PA");
    if ((*pte & (PTE_R | PTE_W)) != (PTE_R | PTE_W)) panic("test_kernel_pagetable: UART wrong perm");
    printf("UART mapping OK.\n");

    // 2. 检查内核代码段的第一个页 (KERNEL_BASE)
    pte = vmem_walk_pte(kernel_root_pagetable, KERNEL_BASE, 0);
    if (pte == 0) panic("test_kernel_pagetable: KERNEL_BASE not mapped");
    if ((*pte & PTE_V) == 0) panic("test_kernel_pagetable: KERNEL_BASE PTE not valid");
    pa = PTE_TO_PA(*pte);
    if (pa != KERNEL_BASE) panic("test_kernel_pagetable: KERNEL_BASE wrong PA");
    if ((*pte & (PTE_R | PTE_X)) != (PTE_R | PTE_X)) panic("test_kernel_pagetable: KERNEL_BASE wrong perm");
    printf("Kernel code mapping OK.\n");

    // 3. 检查内核数据段的第一个页
    uint64 data_start = PAGE_UP((uint64)etext);
    pte = vmem_walk_pte(kernel_root_pagetable, data_start, 0);
    if (pte == 0) panic("test_kernel_pagetable: data section not mapped");
    if ((*pte & PTE_V) == 0) panic("test_kernel_pagetable: data section PTE not valid");
    pa = PTE_TO_PA(*pte);
    if (pa != data_start) panic("test_kernel_pagetable: data section wrong PA");
    if ((*pte & (PTE_R | PTE_W)) != (PTE_R | PTE_W)) panic("test_kernel_pagetable: data section wrong perm");
    printf("Kernel data mapping OK.\n");

    // 4. 检查物理内存顶部的映射
    pte = vmem_walk_pte(kernel_root_pagetable, PHYS_TOP - PAGE_SIZE, 0);
    if (pte == 0) panic("test_kernel_pagetable: PHYS_TOP not mapped");
    if ((*pte & PTE_V) == 0) panic("test_kernel_pagetable: PHYS_TOP PTE not valid");
    pa = PTE_TO_PA(*pte);
    if (pa != PHYS_TOP - PAGE_SIZE) panic("test_kernel_pagetable: PHYS_TOP wrong PA");
    if ((*pte & (PTE_R | PTE_W)) != (PTE_R | PTE_W)) panic("test_kernel_pagetable: PHYS_TOP wrong perm");
    printf("Physical RAM top mapping OK.\n");

    printf_color("=== Kernel pagetable test PASSED ===\n",GREEN);
}

// 启用分页
void vmem_enable_paging(void) {
    sfence_vma();
    w_satp(MAKE_SATP((uint64)kernel_root_pagetable));
    sfence_vma();
    printf_color("vmem_enable_paging: paging enabled.\n",BLACK);
}

// 测试启用分页后，物理内存（高位）能否正常分配，写入，读取，释放
void test_post_paging(void) {
    printf_color("=== Running post-paging test ===\n", BLUE);

    // 1. 尝试分配一页内存
    void *p = kmem_alloc();
    if (p == 0) {
        panic("kmem_alloc failed after paging");
    }
    printf("Post-paging kmem_alloc OK, allocated at PA: %p\n", p);

    // 2. 尝试向这页内存写入数据
    strcpy(p, "Hello Paging World!");
    printf("Post-paging memory write OK.\n");

    // 3. 尝试从这页内存读回数据并验证
    if (strcmp(p, "Hello Paging World!") != 0) {
        panic("Post-paging memory read verification failed");
    }
    printf("Post-paging memory read OK.\n");

    // 4. 尝试释放这页内存
    kmem_free(p);
    printf("Post-paging kmem_free OK.\n");

    printf_color("=== Post-paging test PASSED! ===\n", GREEN);
}

// 尝试向Readonly的代码段进行写入
void test_write_to_readonly(void) {
    printf_color("\n=== Running test: Write to Read-Only Memory ===\n", YELLOW);
    // KERNEL_BASE 是我们内核代码的起始地址，它被映射为只读+可执行。
    char *kernel_code_ptr = (char *) KERNEL_BASE + 20;
    // 1. 首先，尝试读取。这应该是成功的。
    printf("Reading from kernel code @ %p... ", kernel_code_ptr);
    char original_byte = *kernel_code_ptr;
    printf("Value is 0x%x. Read OK.\n", original_byte);
    // 2. 接下来，尝试写入。CPU会顿住，并触发一个异常，不过目前没写异常处理
    printf("Attempting to write 'X' to read-only kernel code @ %p...\n", kernel_code_ptr);
    // CPU 硬件会在这里检测到权限冲突 (W=0)，并触发一个 Store Page Fault！
    *kernel_code_ptr = 'X';
    // 如果代码能执行到这里，说明内存保护没有生效，是一个严重的 Bug！
    printf_color("!!! TEST FAILED: Write to read-only memory did NOT cause a fault! !!!\n", RED);
    // 恢复原来的值 (虽然理论上走不到这里)
    *kernel_code_ptr = original_byte;
}
