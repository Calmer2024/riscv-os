# user/_start.S
#include "syscall.h"

.section .text.start
.globl _start

_start:
    # argc/argv already set by exec via trapframe
    call main

    li a7, SYSCALL_exit  # load exit syscall number
    ecall

_loop:
    j _loop
