#ifndef __MEMLAYOUT_H__
#define __MEMLAYOUT_H__
// 用于描述内存布局

// 页大小 (4KB)
#define PGSIZE 4096 
// 0x1000->0001 0000 0000 0000->0000 1111 1111 1111->1111 0000 0000 0000->0xf000
// 页对齐宏，比如输入0x1234，向上对齐0x2000，向下对齐0x1000；~(PGSIZE-1)生成页掩码：
// 将地址向上或向下对齐到页边界
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

// 物理内存顶部地址 (假设为128MB)
#define PHYSTOP 0x88000000 // 0x80000000 + 128MB

// 内核加载的基地址
#define KERNBASE 0x80200000

#define KERNEL_STACK_PAGES 4
// 内核栈的总大小 (bytes)
#define KERNEL_STACK_SIZE (KERNEL_STACK_PAGES * PGSIZE) // 16KB
// 内核栈的虚拟地址顶部。栈是向下生长的。
#define KERNEL_STACK_TOP 0x80800000
// 栈保护页的起始虚拟地址 (位于栈底之下)
#define KERNEL_STACK_GUARD (KERNEL_STACK_TOP - KERNEL_STACK_SIZE - PGSIZE)

// end 符号由链接脚本 kernel.ld 提供，代表内核代码+数据的末尾
extern char end[];
extern char etext[];

#endif