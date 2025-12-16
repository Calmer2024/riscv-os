/* user/user.ld */
SECTIONS
{
    /* 告诉链接器从虚拟地址 0x0 开始 */
    . = 0x0;

    /* 第一个段：代码 (.text) */
    .text : {
        *(.text.start)
        *(.text .text.*)
    }

    /* * 关键！在 .text 之后，将"当前地址"
     * 对齐到下一个 4K (PAGE_SIZE) 边界
     */
    . = ALIGN(0x1000);

    /* 第二个段：数据 (.data, .bss 等) */
    .data : {
        *(.rodata .rodata.*)
        *(.data .data.*)
        *(.bss .bss.*)
    }
}