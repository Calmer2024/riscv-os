# kernel/entry.S
# QEMU 默认将内核加载到 0x80000000 地址并从这里开始执行。
# 链接脚本 (kernel.ld) 会确保 _entry 标签就在这个地址。
    .section .text
    .global  _entry

_entry:
# 1. 设置栈指针
# kernel/kernel.c 中定义一个叫 stack0 的数组作为栈空间，栈空间在 .bss 段
# la 指令加载 stack0 数组的起始(最低)地址
# sp = stack0 + 4096，这样 sp 就指向了栈的顶部

    la       sp, stack0
    li       t0, 4096
    add      sp, sp, t0

# 2. 清零 .bss 段
# .bss 段存放所有未初始化的全局变量，它们初始值应该为0，不初始化也是0
# 但是应该是qemu帮我们进行清空了，健壮情况下还是应该清零
# 链接脚本会提供 sbss (start of bss) 和 ebss (end of bss) 两个地址标签。

    la       t0, sbss
    la       t1, ebss
bss_loop:
# 使用 sd 一次写入8个字节的0
    la       a0 ,0
    sd       a0, 0(t0)
    addi     t0, t0, 8
    blt      t0, t1, bss_loop

# 3. 跳转到C语言的 main 函数
    call     start

# 4. 防止 main 函数意外返回
# 内核主函数永远不应该返回。
# 如果它真的返回了，让CPU在这里无限循环，防止它执行垃圾指令。
spin:
    j        spin