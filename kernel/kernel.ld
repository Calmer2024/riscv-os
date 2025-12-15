OUTPUT_ARCH("riscv")
ENTRY(_entry)

SECTIONS
{
    /* 内核从这个地址开始。QEMU会把内核加载到这里。 */
    . = 0x80000000;

    /* 代码段 (.text) */
    .text : {
        /* 把所有输入文件的代码部分都放在这里 */
        *(.text .text.*)
        . = ALIGN(0x1000);
        PROVIDE(ptrampoline = .);
        *(.trampsec)
        . = ALIGN(0x1000);
        etext = .;
    }

    /* 只读数据段 (.rodata) */
    .rodata : {
        . = ALIGN(16); /* 16字节对齐，提高访问效率 */
        srodata = .;
        *(.rodata .rodata.*)
        erodata = .;
    }

    /* 用户初始代码段 (.initcode) */
    .initcode : ALIGN(16) {
        PROVIDE(initcode_start = .);
        *(.initcode)
        PROVIDE(initcode_end = .);
    }

    /* 已初始化数据段 (.data) */
    .data : {
        . = ALIGN(16);
/*        标记data段开始*/
        sdata = .;
        *(.data .data.*)
        edata = .;
/*        标记data段结束*/
    }

    /* BSS段 (未初始化数据) */
    .bss : {
        . = ALIGN(16);
        sbss = .; /* 创建一个符号 sbss，标记BSS段的开始 */
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        . = ALIGN(16);
        ebss = .; /* 创建一个符号 ebss，标记BSS段的结束 */
    }

    /* 创建一个符号 end，标记整个内核二进制文件的结束 */
    PROVIDE(end = .);
}