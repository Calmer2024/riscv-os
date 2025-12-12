# 代码段
.section .text.entry
.global _start

_start:
    # 1. [调试] 输出'S' (Start)
    li t0, 0x10000000     # UART基地址
    li t1, 'S'
    sb t1, 0(t0)

    # 2. 设置启动栈
    # 我们根据 mhartid 计算 stack0 中的位置
    # sp = stack0 + (hartid + 1) * 4096
    # 但这里我们直接硬编码单核

    la sp, stack0         # 加载 stack0 数组的基地址
    li t1, 4096           # 每个核心的栈大小 (4KB)
    add sp, sp, t1        # sp = stack0 + 4096 (栈顶)

    # 设置 tp (Thread Pointer) 寄存器
    # xv6 在内核中习惯用 tp 寄存器保存 hartid
    li tp, 0              # tp = 0 (我们是单核)

    # 3. [调试] 输出 'P' (Pointer Set)
    # 重新加载 t0，因为上面的计算可能使用了临时寄存器
    li t0, 0x10000000
    li t1, 'P'
    sb t1, 0(t0)

    # 4. 清零 BSS 段
    la a0, _bss_start
    la a1, _bss_end
    call clear_bss

    # 5. [调试] 输出 'B' (BSS Cleared)
    li t0, 0x10000000
    li t1, 'B'
    sb t1, 0(t0)

    # 6. 跳转到 C 语言 main
    call main

    # 7. [调试] 如果 main 返回了 (不应该发生)，输出 'E'
    li t0, 0x10000000
    li t1, 'E'
    sb t1, 0(t0)

spin:
    j spin

# BSS段清零函数
clear_bss:
    beq a0, a1, clear_done
clear_loop:
    sb zero, 0(a0)
    addi a0, a0, 1
    blt a0, a1, clear_loop
clear_done:
    ret

# ==========================================
# 分配启动栈空间
# ==========================================
# 我们把它放在 .data 段而不是 .bss 段，
# 是为了防止上面的 clear_bss 函数不小心把我们正在使用的栈给清空了。
# (虽然逻辑上 clear_bss 不会清空当前栈帧，但放在 .data 更安全且被 QEMU 自动初始化)

.section .data
.align 4                # 16字节对齐 (2^4 = 16)
.global stack0
stack0:
    .space 4096 * 4     # 为 4 个核心预留 16KB 空间 (NCPU * PGSIZE)