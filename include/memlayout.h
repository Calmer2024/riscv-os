#ifndef MEMLAYOUT_H
#define MEMLAYOUT_H

#include "riscv.h"

#define VIRTIO0 0x10001000

#define UART0 0x10000000L
// UART的中断号为10
#define UART_IRQ 10
// virtio0中断号为1
#define VIRTIO0_IRQ 1

#define KERNBASE 0x80000000L // 内核起始地址
#define PHYSTOP (KERNBASE + 128 * 1024 * 1024) // 内核接管的物理最高地址

#define CLINT           0x2000000L

// PLIC映射的内存地址
#define PLIC 0x0c000000L
#define PLIC_PRIORITY (PLIC + 0x0)
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_SENABLE(hart) (PLIC + 0x2080 + (hart)*0x100)
#define PLIC_SPRIORITY(hart) (PLIC + 0x201000 + (hart)*0x2000)
#define PLIC_SCLAIM(hart) (PLIC + 0x201004 + (hart)*0x2000)

// 用户能访问到的最高虚拟内存地址
#define MAXVA           (1L << 38)
#define MAX_USER_VA (MAX_VIRTUAL_ADDR>>1)
#define USER_STACK_VA (MAX_USER_VA-PAGE_SIZE) //用户栈的虚拟地址

// 内核的虚拟地址空间
#define TRAMPOLINE (MAX_VIRTUAL_ADDR - PAGE_SIZE) // <-- 跳板代码页 (S/U 模式均可访问, 可执行)
#define TRAPFRAME (TRAMPOLINE - PAGE_SIZE) // <-- 中断帧数据页 (struct trapframe)
#define KERNEL_STACK(p) (TRAMPOLINE - ((p)+1)* 2*PAGE_SIZE) // 进程p的内核栈
#define VKSTACK(p)(p) (TRAMPOLINE - ((p)+1)* 2*PAGE_SIZE) // 进程p的内核栈


// 链接脚本提供的符号
extern char etext[]; // 代码段结束
extern char end[];   // 内核结束

#endif //MEMLAYOUT_H