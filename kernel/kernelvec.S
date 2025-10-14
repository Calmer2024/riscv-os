# 作用: 这是硬件在发生中断或异常时，第一个跳转到的地方。它不能是C代码，因为它需要精细地操作寄存器和栈。
# 声明本文件使用的是RISC-V汇编
.altmacro

# 定义一个宏，用于简化寄存器的保存(sd)和恢复(ld)
# REGSAVE rs, offset  -> sd rs, offset(sp)
# REGLOAD rs, offset  -> ld rs, offset(sp)
.macro REGSAVE rs, offset   #存储寄存器到栈
    sd \rs, \offset(sp)
.endm

.macro REGLOAD rs, offset   #从栈加载寄存器
    ld \rs, \offset(sp)
.endm

.section .text
.globl kernelvec
.align 2 # 4字节对齐

kernelvec:
    # -------------------------------------------------------------
    # -- 上下文保存 (Prologue) --
    # 任务: 将所有32个通用寄存器和sepc/sstatus保存到内核栈的陷阱帧中
    # -------------------------------------------------------------

    # 1. 保存所有调用者保存寄存器 (Caller-saved) 和 临时寄存器
    # (ra, t0-t6, a0-a7)
    REGSAVE ra, 40   # ra (x1)
    REGSAVE gp, 56   # gp (x3)
    REGSAVE tp, 64   # tp (x4)
    REGSAVE t0, 72   # t0 (x5)
    REGSAVE t1, 80   # t1 (x6)
    REGSAVE t2, 88   # t2 (x7)
    REGSAVE a0, 112  # a0 (x10)
    REGSAVE a1, 120  # a1 (x11)
    REGSAVE a2, 128  # a2 (x12)
    REGSAVE a3, 136  # a3 (x13)
    REGSAVE a4, 144  # a4 (x14)
    REGSAVE a5, 152  # a5 (x15)
    REGSAVE a6, 160  # a6 (x16)
    REGSAVE a7, 168  # a7 (x17)
    REGSAVE t3, 248  # t3 (x28)
    REGSAVE t4, 256  # t4 (x29)
    REGSAVE t5, 264  # t5 (x30)
    REGSAVE t6, 272  # t6 (x31)

    # 2. 保存所有被调用者保存寄存器
    # (sp, s0-s11)
    # 注意：sp (x2) 需要在最后保存，因为它现在正被用作基址
    REGSAVE s0, 96   # s0 (x8)
    REGSAVE s1, 104  # s1 (x9)
    REGSAVE s2, 176  # s2 (x18)
    REGSAVE s3, 184  # s3 (x19)
    REGSAVE s4, 192  # s4 (x20)
    REGSAVE s5, 200  # s5 (x21)
    REGSAVE s6, 208  # s6 (x22)
    REGSAVE s7, 216  # s7 (x23)
    REGSAVE s8, 224  # s8 (x24)
    REGSAVE s9, 232  # s9 (x25)
    REGSAVE s10, 240 # s10 (x26)
    REGSAVE s11, 248 # s11 (x27) - 注意：riscv-gcc有时会把s11和t3用作同一个寄存器，但分开保存更安全

    # 3. 保存特殊的CSR寄存器 (sepc, sstatus)
    # 因为不是通用寄存器，因此需要通过两条命令来执行
    # 我们需要一个临时寄存器来中转，t0 已经被保存了，可以安全使用
    csrr t0, sepc
    REGSAVE t0, 24   # 保存 sepc 到 trapframe->epc

    # 4. 现在，保存原始的 sp 寄存器
    # 我们需要计算出进入陷阱前的 sp 值。
    # 对于内核陷阱，sp没有切换，所以直接保存当前的sp值即可。
    REGSAVE sp, 48   # 保存 sp (x2)

    # -------------------------------------------------------------
    # -- 调用C语言处理函数 --
    # -------------------------------------------------------------

    mv a0, sp

    # C处理函数 kerneltrap() 不需要参数，它会自己通过sp()找到trapframe
    call kerneltrap

    # -------------------------------------------------------------
    # -- 上下文恢复 --
    # 任务: 从陷阱帧中恢复所有寄存器，准备返回
    # -------------------------------------------------------------

    # 1. 恢复所有通用寄存器 (顺序与保存时相反或无关)
    REGLOAD ra, 40
    REGLOAD sp, 48
    REGLOAD gp, 56
    REGLOAD tp, 64
    REGLOAD t0, 72
    REGLOAD t1, 80
    REGLOAD t2, 88
    REGLOAD s0, 96
    REGLOAD s1, 104
    REGLOAD a0, 112
    REGLOAD a1, 120
    REGLOAD a2, 128
    REGLOAD a3, 136
    REGLOAD a4, 144
    REGLOAD a5, 152
    REGLOAD a6, 160
    REGLOAD a7, 168
    REGLOAD s2, 176
    REGLOAD s3, 184
    REGLOAD s4, 192
    REGLOAD s5, 200
    REGLOAD s6, 208
    REGLOAD s7, 216
    REGLOAD s8, 224
    REGLOAD s9, 232
    REGLOAD s10, 240
    REGLOAD s11, 248
    REGLOAD t3, 248
    REGLOAD t4, 256
    REGLOAD t5, 264
    REGLOAD t6, 272

    # 2. 恢复 sepc
    # sstatus 寄存器由 sret 指令自动恢复，无需手动写回
    REGLOAD t0, 24
    csrw sepc, t0

    # -------------------------------------------------------------
    # -- 从陷阱返回 --
    # -------------------------------------------------------------

    # sret (Supervisor return) 指令会做三件事:
    # 1. 将 PC (程序计数器) 设置为 sepc 的值
    # 2. 将特权级恢复到 sstatus.SPP 中保存的级别 (用户态或内核态)
    # 3. 将中断使能状态恢复到 sstatus.SPIE
    sret