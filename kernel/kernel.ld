/* 内存布局定义 */
SECTIONS {
   /* 确定起始地址，是内核的默认加载地址 */
    . = 0x80200000;
    
    /* 代码段 */
    .text : {
        *(.text) /* 包含的所有文件的代码段 */
    }
    
    /* 只读数据段 */
    .rodata : {
        *(.rodata)
    }
    
    /* 数据段 */
    .data : {
        *(.data)
    }
    
    .bss : {
        _bss_start = .;
        *(.bss)
        _bss_end = .;
    }
    
    /* 栈空间定义 */
    . = ALIGN(16);
    stack_bottom = .;
    . += 0x1000;    /* 分配4KB栈空间 */
    stack_top = .;
}
