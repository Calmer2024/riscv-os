// 作用: 定义进程、上下文和陷阱帧，这是保存CPU状态的“容器”。
#ifndef __PROC_H__
#define __PROC_H__

#include "types.h"
#include "riscv.h"

// CPU核心状态（为未来多核准备）
struct cpu {
    // struct proc *proc; // 当前运行的进程
    int noff;            // 关中断嵌套层数
    int intena;          // 在关中断前，中断是否开启
    uint64 trapstack;
};

extern struct cpu cpus[1]; // 假设只有一个核心

// 陷阱帧：就是一个在内存中创建的数据结构，它的唯一目的就是完整保存当陷阱发生时，CPU 的完整执行状态（上下文）
// 寄存器上下文，由kernelvec.S保存
// 32个通用寄存器 (x0-x31)
// sepc: 发生陷阱时指令的地址
// sstatus: 状态寄存器
struct trapframe {
    uint64 kernel_satp;   // 内核页表
    uint64 kernel_sp;     // 内核栈顶
    uint64 kernel_trap;   // kerneltrap()函数的地址
    uint64 epc;           // sepc寄存器
    uint64 kernel_hartid; // cpuid，我们单核不需要
    // 相关寄存器，存档程序计算时的临时数据、参数、返回值等
    uint64 ra;
    uint64 sp;
    uint64 gp;
    uint64 tp;
    uint64 t0;
    uint64 t1;
    uint64 t2;
    uint64 s0;
    uint64 s1;
    uint64 a0;
    uint64 a1;
    uint64 a2;
    uint64 a3;
    uint64 a4;
    uint64 a5;
    uint64 a6;
    uint64 a7;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
    uint64 t3;
    uint64 t4;
    uint64 t5;
    uint64 t6;
};

void proc_init(void);

#endif