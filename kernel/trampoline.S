# trampoline.S
#
# 低级代码，用于在用户模式和内核模式之间切换。
# 必须映射到 TRAMPOLINE 指向的同一个虚拟地址，
# 且该页要在用户页表和内核页表中都存在。

.section .tramp, "ax"
.globl trampoline
trampoline:
.align 4
.globl uservec
uservec:
    #
    # trap from user mode.
    #
    # 当从用户模式进入这里时：
    # 1. satp 指向用户页表
    # 2. sscratch 指向当前进程的 trapframe (位于虚拟地址 TRAPFRAME)
    # 3. sp 指向用户栈
    #

    # 1. 交换 a0 和 sscratch
    #    此时：a0 = TRAPFRAME 地址, sscratch = 原本的 a0 值
    csrrw a0, sscratch, a0      # a0 = TRAPFRAME, sscratch = user a0

    sd ra, 40(a0)
    sd sp, 48(a0)
    sd gp, 56(a0)
    sd tp, 64(a0)
    sd t0, 72(a0)
    sd t1, 80(a0)
    sd t2, 88(a0)
    sd s0, 96(a0)
    sd s1, 104(a0)

    # 保存 user a0
    csrr t0, sscratch
    sd t0, 112(a0)

    sd a1, 120(a0)
    sd a2, 128(a0)
    sd a3, 136(a0)
    sd a4, 144(a0)
    sd a5, 152(a0)
    sd a6, 160(a0)
    sd a7, 168(a0)

    sd s2, 176(a0)
    sd s3, 184(a0)
    sd s4, 192(a0)
    sd s5, 200(a0)
    sd s6, 208(a0)
    sd s7, 216(a0)
    sd s8, 224(a0)
    sd s9, 232(a0)
    sd s10, 240(a0)
    sd s11, 248(a0)

    sd t3, 256(a0)
    sd t4, 264(a0)
    sd t5, 272(a0)
    sd t6, 280(a0)

    # 恢复内核环境
    ld sp, 8(a0)      # kernel_sp
    ld tp, 32(a0)     # kernel_hartid
    ld t0, 16(a0)     # kernel_trap
    ld t1, 0(a0)      # kernel_satp

    csrw satp, t1
    sfence.vma zero, zero
    jr t0


.globl userret
userret:
    #
    # return to user mode.
    #
    # 由 usertrapret() 调用
    # a0: TRAPFRAME (在用户页表中的虚拟地址)
    # a1: user page table satp value
    #

    # 1. 切换页表到 User Page Table
    #    注意：我们还在 trampoline 页上执行，这个页在用户/内核页表都有映射，所以是安全的
    csrw satp, a1
    sfence.vma zero, zero
    csrw sscratch, a0           # sscratch = TRAPFRAME

    ld ra, 40(a0)
    ld sp, 48(a0)
    ld gp, 56(a0)
    ld tp, 64(a0)
    ld t0, 72(a0)
    ld t1, 80(a0)
    ld t2, 88(a0)
    ld s0, 96(a0)
    ld s1, 104(a0)

    ld a0, 112(a0)
    ld a1, 120(a0)
    ld a2, 128(a0)
    ld a3, 136(a0)
    ld a4, 144(a0)
    ld a5, 152(a0)
    ld a6, 160(a0)
    ld a7, 168(a0)

    ld s2, 176(a0)
    ld s3, 184(a0)
    ld s4, 192(a0)
    ld s5, 200(a0)
    ld s6, 208(a0)
    ld s7, 216(a0)
    ld s8, 224(a0)
    ld s9, 232(a0)
    ld s10, 240(a0)
    ld s11, 248(a0)

    ld t3, 256(a0)
    ld t4, 264(a0)
    ld t5, 272(a0)
    ld t6, 280(a0)

    # 返回用户模式
    sret
