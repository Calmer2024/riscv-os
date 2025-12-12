OUTPUT_ARCH(riscv)
ENTRY(_start)

/*
 * 用户程序的虚拟地址从 0x1000 (4KB) 开始。
 * 0x0 - 0x1000 是空页，用于捕获 NULL 指针解引用。
 */
BASE_ADDRESS = 0x1000;

SECTIONS
{
  . = BASE_ADDRESS;

  /* 代码段 (Text Section) */
  .text : {
    /* 用户程序的入口点 */
    *(.text.entry)
    *(.text .text.*)
  }

  /* 只读数据段 (Read-only Data Section) */
  .rodata : {
    . = ALIGN(4096); /* 页对齐 */
    *(.rodata .rodata.*)
  }

  /* 数据段 (Data Section) */
  .data : {
    . = ALIGN(4096); /* 页对齐 */
    *(.data .data.*)
  }

  /* BSS段 (Uninitialized Data) */
  /* 我们需要 .bss 段，但用户程序启动代码 (usertests) 会自己清零 */
  .bss : {
    . = ALIGN(4096); /* 页对齐 */
    *(.bss .bss.*)
  }

  /* * .stack 段不是必须的，因为栈是由内核分配的。
   * end 符号标记了程序占用的末尾。
   */
  . = ALIGN(4096);
  PROVIDE(end = .);
}