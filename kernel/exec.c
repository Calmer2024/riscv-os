#include "../include/elf.h"
#include "../include/kalloc.h"
#include "../include/memlayout.h"
#include "../include/param.h"
#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/string.h"
#include "../include/vm.h"

// // 嵌入内核的c语言用户程序（usertest.elf）
// extern char _binary_kernel_usertest_elf_start[];
// extern char _binary_kernel_usertest_elf_size[];


// 辅助函数：将 ELF 的 flags 转换为 RISC-V PTE 的 flags
static int elf_flags_to_pte_flags(uint32 elf_flags) {
    int pte_flags = PTE_U; // 必须是用户页

    if (elf_flags & ELF_PROG_FLAG_READ) {
        pte_flags |= PTE_R;
    }
    if (elf_flags & ELF_PROG_FLAG_WRITE) {
        pte_flags |= PTE_W;
    }
    if (elf_flags & ELF_PROG_FLAG_EXEC) {
        pte_flags |= PTE_X;
    }
    return pte_flags;
}

// 从inode读取文件内容
static int load_elf_from_inode(struct inode *ip, pagetable_t *out_pagetable, uint64 *out_sz, uint64 *out_entry) {
    struct elfhdr elf;
    struct proghdr ph;
    pagetable_t pagetable = 0;
    uint64 max_va = 0;

    // 1. 读取 ELF 头 (从文件偏移 0 开始)
    //    注意：fs_inode_read_data 的第二个参数 0 表示目标地址是内核地址(dst)
    if (fs_inode_read_data(ip, 0, (char *)&elf, 0, sizeof(elf)) != sizeof(elf)) {
        goto bad;
    }

    // 2. 校验魔数
    if (elf.magic != ELF_MAGIC) {
        printf("exec: elf magic invalid\n");
        goto bad;
    }

    // 3. 创建新页表
    pagetable = vmem_create_pagetable();
    if (pagetable == 0) goto bad;

    // 4. 遍历并加载程序段
    for (int i = 0; i < elf.phnum; i++) {
        // 读取第 i 个程序头
        uint64 ph_off = elf.phoff + i * sizeof(ph);
        if (fs_inode_read_data(ip, 0, (char *)&ph, ph_off, sizeof(ph)) != sizeof(ph)) {
            goto bad;
        }

        if (ph.type != ELF_PROG_LOAD) continue;
        if (ph.memsz < ph.filesz) goto bad;
        if (ph.vaddr % PAGE_SIZE != 0) goto bad; // 简化处理：要求页对齐

        int pte_flags = elf_flags_to_pte_flags(ph.flags);

        // 为该段分配内存并加载数据
        for (uint64 va = ph.vaddr; va < ph.vaddr + ph.memsz; va += PAGE_SIZE) {
            char *pa = kmem_alloc();
            if (pa == 0) goto bad;

            memset(pa, 0, PAGE_SIZE); // 清零 (处理 .bss)

            // 计算这一页需要从文件读多少字节
            // 如果 va >= vaddr + filesz，说明全是 bss，不用读
            uint64 offset_in_segment = va - ph.vaddr;
            if (offset_in_segment < ph.filesz) {
                uint64 bytes_to_read = ph.filesz - offset_in_segment;
                if (bytes_to_read > PAGE_SIZE) bytes_to_read = PAGE_SIZE;

                // 从文件读取内容到物理页 pa
                if (fs_inode_read_data(ip, 0, pa, ph.off + offset_in_segment, bytes_to_read) != bytes_to_read) {
                    kmem_free(pa);
                    goto bad;
                }
            }

            // 映射
            if (vmem_map_pagetable(pagetable, va, (uint64)pa, pte_flags) != 0) {
                kmem_free(pa);
                goto bad;
            }
        }

        uint64 end = ph.vaddr + ph.memsz;
        if (end > max_va) max_va = end;
    }

    *out_pagetable = pagetable;
    *out_sz = PAGE_UP(max_va);
    *out_entry = elf.entry; // 返回入口地址
    return 0;

bad:
    if (pagetable) proc_free_pagetable(pagetable, PAGE_UP(max_va));
    return -1;
}


// =================================================================
// 新的 Exec 系统调用实现
// path: 可执行文件路径
// argv: 参数数组 (字符串指针数组，以NULL结尾)
// =================================================================
uint64 exec(char *path, char **argv) {
    struct inode *ip;
    pagetable_t new_pagetable = 0, old_pagetable;
    uint64 new_sz = 0, old_sz;
    uint64 entry_pc = 0;
    struct proc *p = proc_running();

    extern char trampoline[];

    // 1. 查找文件 (Path Lookup)
    if ((ip = fs_namei(path)) == 0) {
        // printf("exec: %s not found\n", path);
        return -1;
    }

    // 锁定 Inode 以便读取
    fs_inode_lock(ip);
    fs_inode_read(ip);

    // 2. 加载 ELF
    if (load_elf_from_inode(ip, &new_pagetable, &new_sz, &entry_pc) < 0) {
        printf("exec: load failed\n");
        fs_inode_unlock(ip);
        fs_inode_release(ip);
        return -1;
    }

    // 加载完成，释放 inode (不用一直拿着锁)
    fs_inode_unlock(ip);
    fs_inode_release(ip);

    // 3. 分配用户栈 (User Stack)
    // 栈顶设在 MAX_USER_VA
    // 我们分配 1 页作为栈
    uint64 stack_base = MAX_USER_VA - PAGE_SIZE;
    char *stack_pa = kmem_alloc();
    if (!stack_pa) goto bad;

    if (vmem_map_pagetable(new_pagetable, stack_base, (uint64)stack_pa, PTE_U | PTE_R | PTE_W) != 0) {
        kmem_free(stack_pa);
        goto bad;
    }

    // 4. 处理参数 (Argv) -> 压入新栈
    // 这是一个精细活：我们需要把字符串从旧页表拷贝到新页表的栈上
    uint64 sp = MAX_USER_VA;
    uint64 ustack[MAXARG]; // 用来暂存参数在新栈上的地址
    int argc = 0;

    for (argc = 0; argv[argc]; argc++) {
        if (argc >= MAXARG) goto bad;

        // 计算长度 (包括 \0)
        int len = strlen(argv[argc]) + 1;
        sp -= len;
        // 16字节对齐 (RISC-V 栈对齐要求)
        sp -= (sp % 16);

        if (sp < stack_base) goto bad; // 栈溢出

        // 将字符串拷贝到新栈 (这里我们直接操作物理内存 stack_pa 会更简单，但为了通用性用 copyout)
        // 注意：目前切页表还没发生，所以 copyout 需要目标页表 new_pagetable
        // 但我们没有 copyout_to_pagetable 这种函数，通常 copyout 只能往当前页表写。
        // TRICK: 我们直接算物理地址偏移写进去！
        uint64 offset = sp - stack_base;
        memmove(stack_pa + offset, argv[argc], len);

        ustack[argc] = sp; // 记录字符串的新地址
    }
    ustack[argc] = 0; // argv[argc] = 0 (NULL terminated)

    // 接着把 ustack 指针数组压入栈
    sp -= (argc + 1) * sizeof(uint64);
    sp -= (sp % 16);
    if (sp < stack_base) goto bad;

    // 把 ustack 数组拷贝到栈上
    uint64 offset = sp - stack_base;
    memmove(stack_pa + offset, ustack, (argc + 1) * sizeof(uint64));

    // 记录 main(argc, argv) 的参数
    // RISC-V 约定: a0 = argc, a1 = argv
    p->trapframe->a0 = argc;
    p->trapframe->a1 = sp;


    // 5. 映射 Trampoline 和 Trapframe
    // 这里的逻辑和之前一样
    vmem_map_pagetable(new_pagetable, TRAMPOLINE, (uint64)trampoline, PTE_X | PTE_R);
    vmem_map_pagetable(new_pagetable, TRAPFRAME, (uint64)p->trapframe, PTE_W | PTE_R);

    // 6. 提交修改 (Commit Point)
    old_pagetable = p->pagetable;
    old_sz = p->size;

    p->pagetable = new_pagetable;
    p->size = new_sz;
    p->trapframe->epc = entry_pc; // 设置入口点
    p->trapframe->sp = sp;        // 设置新栈顶

    // 释放旧页表
    proc_free_pagetable(old_pagetable, old_sz);

    // 拷贝名字用于调试 (可选)
    // safestrcpy(p->name, path, sizeof(p->name));

    return 0; // exec 不返回，从新入口开始跑

bad:
    if (new_pagetable) proc_free_pagetable(new_pagetable, new_sz);
    return -1;
}
