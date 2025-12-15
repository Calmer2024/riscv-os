#ifndef __SYSCALL_H__
#define __SYSCALL_H__

// -----------------------------------------------------------------
//
// 这一部分是 C 和汇编共享的
//
// -----------------------------------------------------------------

// 系统调用号
#define SYS_exit    1
#define SYS_fork    2
#define SYS_wait    3
#define SYS_kill    4
#define SYS_getpid  5
#define SYS_open    6
#define SYS_close   7
#define SYS_read    8
#define SYS_write   9
#define SYS_sbrk    10
#define SYS_test    11


// -----------------------------------------------------------------
//
// 这一部分是 仅 C 语言 (C-Only) 可见的
// 汇编器 (Assembler) 会跳过这一部分
//
// -----------------------------------------------------------------

#ifndef __ASSEMBLER__ // 如果不是汇编器

// C 编译器需要这个函数原型
void syscall_dispatch(void);

#endif // __ASSEMBLER__

#endif // __SYSCALL_H__