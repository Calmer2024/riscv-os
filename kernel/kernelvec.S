# kernel/trap/kernelvec.S

    .section .text
    .globl   kernelvec
    .align   4

kernelvec:
# 1. 保存所有通用寄存器到栈上

# 1.a 为什么 s0-s11 (被调用者保存) 没有被保存？
# xv6 相信使用的 C 编译器 (GCC) 会严格遵守 RISC-V 的函数调用规范 (ABI)。
# ABI 的规定：这个规范规定，如果一个函数
# （比如 kerneltrap 或者它调用的任何子函数）
# 想要使用 s0-s11 这些寄存器，那么编译器必须
# 在函数的开头（prologue）生成汇编代码
# 来把这些寄存器的原始值保存到栈上，
# 并在函数的结尾（epilogue）把它们恢复回来。
# xv6 的设计者们认为，既然 C 编译器已经保证会处理 s0-s11，
# 那么 kernelvec 这个汇编入口就没有必要再做一遍重复的工作了。
# 只保存那些 C 编译器默认会“随意破坏”的调用者保存寄存器 (a0-a7, t0-t6等)，
# 是一种性能优化，减少了不必要的内存读写。

# 1.b 为什么 sp (栈指针) 的保存也被注释掉了？
# addi sp, sp, -256：在函数入口，将 sp 向下移动 256 字节，开辟出一块栈帧。
# (C 函数调用)：call kerneltrap。根据 ABI，
# kerneltrap 函数可以自由使用这块 256 字节下方的栈空间，
# 但当它 ret 返回时，必须保证 sp 的值和它被调用时完全一样。
# addi sp, sp, 256：在函数出口，
# kernelvec 又将 sp 向上移动不多不少正好 256 字节。

# sp 是栈指针，先预留出 32 * 8 = 256 字节的空间
    addi     sp, sp, -256

# 保存所有通用寄存器到栈上
    sd       ra, 0(sp)
    sd       sp, 8(sp)
    sd       gp, 16(sp)
    sd       tp, 24(sp)
    sd       t0, 32(sp)
    sd       t1, 40(sp)
    sd       t2, 48(sp)
    sd       s0, 56(sp)
    sd       s1, 64(sp)
    sd       a0, 72(sp)
    sd       a1, 80(sp)
    sd       a2, 88(sp)
    sd       a3, 96(sp)
    sd       a4, 104(sp)
    sd       a5, 112(sp)
    sd       a6, 120(sp)
    sd       a7, 128(sp)
    sd       s2, 136(sp)
    sd       s3, 144(sp)
    sd       s4, 152(sp)
    sd       s5, 160(sp)
    sd       s6, 168(sp)
    sd       s7, 176(sp)
    sd       s8, 184(sp)
    sd       s9, 192(sp)
    sd       s10, 200(sp)
    sd       s11, 208(sp)
    sd       t3, 216(sp)
    sd       t4, 224(sp)
    sd       t5, 232(sp)
    sd       t6, 240(sp)

# 调用 C 语言的陷阱处理函数
    call     trap_kernel

# 3. 从栈上恢复所有寄存器
    ld       ra, 0(sp)
    ld       sp, 8(sp)
    ld       gp, 16(sp)
    ld       tp, 24(sp)
    ld       t0, 32(sp)
    ld       t1, 40(sp)
    ld       t2, 48(sp)
    ld       s0, 56(sp)
    ld       s1, 64(sp)
    ld       a0, 72(sp)
    ld       a1, 80(sp)
    ld       a2, 88(sp)
    ld       a3, 96(sp)
    ld       a4, 104(sp)
    ld       a5, 112(sp)
    ld       a6, 120(sp)
    ld       a7, 128(sp)
    ld       s2, 136(sp)
    ld       s3, 144(sp)
    ld       s4, 152(sp)
    ld       s5, 160(sp)
    ld       s6, 168(sp)
    ld       s7, 176(sp)
    ld       s8, 184(sp)
    ld       s9, 192(sp)
    ld       s10, 200(sp)
    ld       s11, 208(sp)
    ld       t3, 216(sp)
    ld       t4, 224(sp)
    ld       t5, 232(sp)
    ld       t6, 240(sp)

# 恢复栈指针
    addi     sp, sp, 256

# 陷阱返回
    sret

# CPU执行的操作：
# 将sepc寄存器的值写入程序计数器（PC）
# 根据sstatus寄存器中的SPP字段恢复之前的特权级别
# 清除sstatus中的SIE位（禁用监督级中断）
# 设置sstatus中的SPIE位（保存之前的中断使能状态）