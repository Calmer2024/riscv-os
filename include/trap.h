#ifndef __TRAP_H__
#define __TRAP_H__

#include "types.h"

// 中断处理函数指针类型
// 无参数（通过全局寄存器/内存获取中断信息）
// 无返回值（通过修改寄存器/内存传递结果）
typedef void (*interrupt_handler_t)(void);

// RISC-V S-Mode 中断号定义
// 映射的表即为中断向量表
// 与 scause 寄存器的值对应（最高位为1，中断号取低四位；最高位不为1，为异常代码）
enum riscv_interrupts {
    IRQ_S_SOFT   = 1,  // S-Mode 软件中断
    IRQ_S_TIMER  = 5,  // S-Mode 时钟中断
    IRQ_S_EXT    = 9,  // S-Mode 外部中断
    // ...
    MAX_IRQ      = 16  // 中断号上限，假设最多支持16种中断
};
// 中断控制接口
void usertrap(void);
void usertrapret(void);
void kerneltrap(struct trapframe *tf);
void trap_init_hart(void); // 初始化当前CPU核心的中断系统
void register_interrupt_handler(int irq, interrupt_handler_t handler); // 中断注册函数
void unregister_interrupt_handler(int irq); // 中断注销函数

#endif