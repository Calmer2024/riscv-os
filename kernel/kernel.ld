OUTPUT_ARCH(riscv)
ENTRY(_start) /* _start 符号是程序的入口点，在 entry.S 中定义 */

BASE_ADDRESS = 0x80200000;

/* 定义程序头(Segments)，并分别指定权限 */
PHDRS
{
  text PT_LOAD FLAGS(5);   /* R=4, X=1 -> R+X=5 */
  rodata PT_LOAD FLAGS(4); /* R=4 */
  data PT_LOAD FLAGS(6);   /* R=4, W=2 -> R+W=6 */
}

SECTIONS
{
  . = BASE_ADDRESS;

  /* 代码段 (Text Section) */
  .text : {
    *(.text.entry) /* 确保入口代码在最前面 */
    *(.text .text.*)
    . = ALIGN(4096);
    etext = .;
  } :text /* 指派到 text segment */

  /* 只读数据段 (Read-only Data Section) */
  .rodata : {
    . = ALIGN(4096); /* 页对齐，非常重要 */
    *(.rodata .rodata.*)
  } :rodata /* 指派到 rodata segment */

  /* 数据段 (Data Section) */
  .data : {
    . = ALIGN(4096); /* 页对齐 */
    *(.data .data.*)
  } :data /* 指派到 data segment */
  
  /* BSS段 (Uninitialized Data) */
  .bss : {
    . = ALIGN(4096); /* 页对齐 */
    _bss_start = .;
    *(.bss .bss.*)
    _bss_end = .;
  } :data /* BSS段是可写的，所以也放入 data segment */

  /* 定义 end 符号，表示内核使用的所有内存的末尾 */
    . = ALIGN(4096);
    PROVIDE(end = .);
}