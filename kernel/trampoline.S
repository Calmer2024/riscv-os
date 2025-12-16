# 把接下来的所有代码都放入一个叫做 "trampsec" 的段里
#include "../include/memlayout.h"

    .section   .trampsec
    .globl     trampoline
    .globl     usertrap
    .globl     uservec
    .globl     userret

trampoline:

uservec:
# 处理从用户空间进入内核的陷阱
# 内核将包含此代码的页面映射到相同的虚拟地址 (TRAMPOLINE)
# 在用户和内核空间中，以便在切换页表时继续工作
# kernel.ld 确保此代码从页面边界开始

# 保存用户的 a0 寄存器到 sscratch，以便 a0 可以用来访问 TRAPFRAME
    csrw       sscratch, a0

# 每个进程都有一个单独的 p->trapframe 内存区域，
# 但它在每个进程的用户页表中映射到相同的虚拟地址 (TRAPFRAME这个哪里来？)
    li         a0, TRAPFRAME

# 将用户寄存器保存到 TRAPFRAME
    sd         ra, 40(a0)
    sd         sp, 48(a0)
    sd         gp, 56(a0)
    sd         tp, 64(a0)
    sd         t0, 72(a0)
    sd         t1, 80(a0)
    sd         t2, 88(a0)
    sd         s0, 96(a0)
    sd         s1, 104(a0)
    sd         a1, 120(a0)
    sd         a2, 128(a0)
    sd         a3, 136(a0)
    sd         a4, 144(a0)
    sd         a5, 152(a0)
    sd         a6, 160(a0)
    sd         a7, 168(a0)
    sd         s2, 176(a0)
    sd         s3, 184(a0)
    sd         s4, 192(a0)
    sd         s5, 200(a0)
    sd         s6, 208(a0)
    sd         s7, 216(a0)
    sd         s8, 224(a0)
    sd         s9, 232(a0)
    sd         s10, 240(a0)
    sd         s11, 248(a0)
    sd         t3, 256(a0)
    sd         t4, 264(a0)
    sd         t5, 272(a0)
    sd         t6, 280(a0)
# 保存用户的 a0 到 p->trapframe->a0
    csrr       t0, sscratch
    sd         t0, 112(a0)

# 初始化内核栈 p->trapframe->kernel_sp（内核态返回的时候写入其中的）
    ld         sp, 8(a0)
# cpu的id号（目前不知道什么用）, p->trapframe->kernel_hartid
    ld         tp, 32(a0)
# usertrap()首地址, p->trapframe->kernel_trap
    ld         t0, 16(a0)
# 内核页表, p->trapframe->kernel_satp.
    ld         t1, 0(a0)
# 切换到内核页表
    sfence.vma zero, zero
    csrw       satp, t1
    sfence.vma zero, zero
# 跳转到 usertrap()
    jalr       t0


userret:
# 返回用户空间，必须把用户页表放在a0
# 切换到用户页表
    sfence.vma zero, zero
    csrw       satp, a0
    sfence.vma zero, zero

    li         a0,TRAPFRAME

# 恢复用户寄存器
    ld         ra, 40(a0)
    ld         sp, 48(a0)
    ld         gp, 56(a0)
    ld         tp, 64(a0)
    ld         t0, 72(a0)
    ld         t1, 80(a0)
    ld         t2, 88(a0)
    ld         s0, 96(a0)
    ld         s1, 104(a0)
    ld         a1, 120(a0)
    ld         a2, 128(a0)
    ld         a3, 136(a0)
    ld         a4, 144(a0)
    ld         a5, 152(a0)
    ld         a6, 160(a0)
    ld         a7, 168(a0)
    ld         s2, 176(a0)
    ld         s3, 184(a0)
    ld         s4, 192(a0)
    ld         s5, 200(a0)
    ld         s6, 208(a0)
    ld         s7, 216(a0)
    ld         s8, 224(a0)
    ld         s9, 232(a0)
    ld         s10, 240(a0)
    ld         s11, 248(a0)
    ld         t3, 256(a0)
    ld         t4, 264(a0)
    ld         t5, 272(a0)
    ld         t6, 280(a0)
# 恢复用户的 a0
    ld         a0, 112(a0)

# 返回用户模式并跳转到用户 pc
# usertrapret() 需要设置好 sstatus 以及 sepc.
    sret