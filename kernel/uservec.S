#include "../include/riscv.h"  // 包含 SSTATUS_SIE 等

.section .text
.globl uservec
.align 2

uservec:
    # 1. 切换栈
    # 假设 C 代码在创建进程时，已将 p->trapframe 的地址存入 sscratch
    # csrrw sp, sscratch, sp
    # 结果:
    #   sp = p->trapframe (内核栈基址)
    #   sscratch = user_sp (用户栈指针)
    csrrw sp, sscratch, sp

    # 2. 保存所有31个用户 GPRs 到 trapframe
    # (偏移量来自 proc.h)
    # 注意：我们还没有保存 user_sp (它在 sscratch 中)
    sd ra, 40(sp)
    sd gp, 56(sp)
    sd tp, 64(sp)
    sd t0, 72(sp)
    sd t1, 80(sp)
    sd t2, 88(sp)
    sd s0, 96(sp)
    sd s1, 104(sp)
    sd a0, 112(sp)
    sd a1, 120(sp)
    sd a2, 128(sp)
    sd a3, 136(sp)
    sd a4, 144(sp)
    sd a5, 152(sp)
    sd a6, 160(sp)
    sd a7, 168(sp)
    sd s2, 176(sp)
    sd s3, 184(sp)
    sd s4, 192(sp)
    sd s5, 200(sp)
    sd s6, 208(sp)
    sd s7, 216(sp)
    sd s8, 224(sp)
    sd s9, 232(sp)
    sd s10, 240(sp)
    sd s11, 248(sp)
    sd t3, 256(sp)
    sd t4, 264(sp)
    sd t5, 272(sp)
    sd t6, 280(sp)

    # 3. 保存 CSRs 和其他信息
    # 3a. 保存 sepc (异常PC)
    csrr t0, sepc
    sd t0, 24(sp)

    # 3b. **修正: 保存 user_sp (它还安全地在 sscratch 中)**
    # 现在所有 GPR 都保存好了，我们可以安全使用 t0 作为临时寄存器
    csrr t0, sscratch    # (t0 = user_sp)
    sd t0, 48(sp)        # (保存 t0 到 trapframe->sp, 偏移量 48)

    # 4. 填充 C 代码需要访问的内核信息
    csrr t0, satp
    sd t0, 0(sp)    # kernel_satp
    sd sp, 8(sp)    # kernel_sp (就是 trapframe 自己的地址)

    # 5. 调用 C 陷阱处理函数 (trap.c 中的 usertrap)
    #    usertrap 会在结尾调用 usertrapret
    call usertrap

.globl usertrapret
usertrapret:
    # 1. 恢复 CSRs (sepc, sstatus)
    # 1a. 恢复 sepc
    ld t0, 24(sp)
    csrw sepc, t0

    # 1b. 恢复 sstatus (允许中断)
    # (这假设我们是从用户态来的，所以 SPIE 总是 1)
    li t0, SSTATUS_SIE
    csrs sstatus, t0

    # 2. 恢复所有 GPRs
    ld ra, 40(sp)
    ld gp, 56(sp)
    ld tp, 64(sp)
    ld t0, 72(sp)
    ld t1, 80(sp)
    ld t2, 88(sp)
    ld s0, 96(sp)
    ld s1, 104(sp)
    ld a0, 112(sp)
    ld a1, 120(sp)
    ld a2, 128(sp)
    ld a3, 136(sp)
    ld a4, 144(sp)
    ld a5, 152(sp)
    ld a6, 160(sp)
    ld a7, 168(sp)
    ld s2, 176(sp)
    ld s3, 184(sp)
    ld s4, 192(sp)
    ld s5, 200(sp)
    ld s6, 208(sp)
    ld s7, 216(sp)
    ld s8, 224(sp)
    ld s9, 232(sp)
    ld s10, 240(sp)
    ld s11, 248(sp)
    ld t3, 256(sp)
    ld t4, 264(sp)
    ld t5, 272(sp)
    ld t6, 280(sp)

    # 3. 恢复 user sp
    # (注意：sp 必须是最后一个被恢复的 GPR)
    ld sp, 48(sp)

    # 4. sret 返回用户空间
    sret