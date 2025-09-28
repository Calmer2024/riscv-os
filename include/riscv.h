#ifndef __RISCV_H__
#define __RISCV_H__

#include "types.h"
// Sv39页表机制下虚拟地址的分解
/*
| 63 ..... 39 |  38 .. 30  |  29 .. 21  |  20 .. 12  |  11 .. 0   |
|-------------|------------|------------|------------|------------|
|    Unused   |   VPN[2]   |   VPN[1]   |   VPN[0]   | Page Offset|
| (Must be 0) | (9 bits)   | (9 bits)   | (9 bits)   | (12 bits)  |
 */

// PTE结构
/*
63      54 53      28 27      10 9   8 7 6 5 4 3 2 1 0
| Reserved |   PPN   | Reserved |D|A|G|U|X|W|R|V|
 */

// --- Sv39 页表项 (PTE) 和 页目录项 (PDE) 标志位 ---
// 1L表示值为1的long类型，64位整数，1L<<n即生成第n位为1的掩码
#define PTE_V (1L << 0) // Valid
#define PTE_R (1L << 1) // Read
#define PTE_W (1L << 2) // Write
#define PTE_X (1L << 3) // Execute
#define PTE_U (1L << 4) // User

// --- 地址转换宏 ---
// 将物理地址转换为PTE中的PPN字段
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
// 从PTE中提取物理地址
#define PTE2PA(pte) ((((pte) >> 10) << 12))

// --- 虚拟地址索引提取 ---
// level是0，1，2级索引，va是虚拟地址，分别取n级页表索引
#define PXMASK 0x1FF // 9 bits
#define PX(level, va) ((((uint64)va) >> (12 + (level) * 9)) & PXMASK)

// --- satp 寄存器操作 ---（satp寄存器是RISC-V架构中控制虚拟内存系统的核心寄存器）
// stap寄存器结构
/*
63      60 59                  44 43                0
|  MODE   |         ASID         |        PPN         |
 */
#define SATP_MODE_SV39 (8L << 60) //这里是控制stap寄存器分页模式控制的MODE段，设置为Sv39
// 页表基址配置
#define MAKE_SATP(pagetable) (SATP_MODE_SV39 | (((uint64)pagetable) >> 12))

// --- satp 寄存器操作 ---
static inline void w_satp(uint64 x) {
    asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64 r_satp() {
    uint64 x;
    asm volatile("csrr %0, satp" : "=r" (x));
    return x;
}

// --- TLB 刷新 ---
static inline void sfence_vma() {
    // 刷新所有TLB项
    asm volatile("sfence.vma zero, zero");
}

#endif