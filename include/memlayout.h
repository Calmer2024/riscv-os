// include/memlayout.h
#ifndef __MEMLAYOUT_H__
#define __MEMLAYOUT_H__

// ==========================================
// 1. 物理内存布局 (Physical Memory Layout)
//    这些地址由 QEMU 的 "virt" 机器模型决定
// ==========================================

#define UART0           0x10000000L
#define UART0_IRQ       10

#define VIRTIO0         0x10001000L
#define VIRTIO0_IRQ     1

#define CLINT           0x2000000L
#define PLIC            0x0c000000L

// 物理内存起始地址（内核加载的位置）
#define KERNBASE        0x80200000L
// 物理内存结束地址 (128MB)
// 物理 RAM 通常从 0x80000000 开始。
// 即使内核加载在 0x80200000，物理内存的硬件上限依然是 0x80000000 + 128M
#define PHY_RAM_BASE    0x80000000L
#define PHYSTOP         (PHY_RAM_BASE + 128*1024*1024)

// ==========================================
// 2. 虚拟内存布局 (Virtual Memory Layout)
//    RISC-V Sv39 模式
// ==========================================

#define PGSIZE          4096

// RISC-V Sv39 模式下的最大虚拟地址 (2^38 - 1 ≈ 256GB)
#define MAXVA           (1L << 38)

// --- 内核的高端映射区 ---

// 1. 蹦床页面 (Trampoline)
//    映射在虚拟地址的最顶端。用于进出内核的“跳板”。
#define TRAMPOLINE      (MAXVA - PGSIZE)

// 2. 陷阱帧 (Trapframe)
//    紧挨着蹦床下面。用于保存用户进程的寄存器。
#define TRAPFRAME       (TRAMPOLINE - PGSIZE)

// 3. 内核栈 (Kernel Stacks)
//    每个进程都有一个独立的内核栈，映射在高地址。
//    VKSTACK 计算公式：根据进程索引 p 计算其栈的虚拟地址
//    布局：[Guard Page] [Stack] [Guard Page] [Stack] ... [Trampoline]
#define VKSTACK(p)      (TRAMPOLINE - ((p)+1) * 2 * PGSIZE)

// 链接脚本提供的符号
extern char etext[]; // 代码段结束
extern char end[];   // 内核结束

// 辅助宏
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#endif