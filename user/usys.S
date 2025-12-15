#include "../include/syscall.h"

# 定义一个宏来创建桩代码
.macro SYSCALL name, num
    .globl \name
\name:
    li a7, \num
    ecall
    ret
.endm

.section .text

SYSCALL fork, SYS_fork
SYSCALL exit, SYS_exit
SYSCALL wait, SYS_wait
SYSCALL kill, SYS_kill
SYSCALL getpid, SYS_getpid
SYSCALL open, SYS_open
SYSCALL close, SYS_close
SYSCALL read, SYS_read
SYSCALL write, SYS_write
SYSCALL sbrk, SYS_sbrk