# user/usys.S
# 包含你的系统调用号定义
#include "syscall.h"

# 声明所有函数为全局，这样 C 才能链接到它们
    .globl getpid
    .globl fork
    .globl wait
    .globl exit
    .globl exec
    .globl open
    .globl close
    .globl read
    .globl write
    .globl fstat
    .globl mkdir
    .globl sysinfo
    .globl sem_open
    .globl sem_wait
    .globl sem_signal
    .globl sbrk
    .globl fslog_crash
    .globl chdir
    .globl pipe
    .globl link
    .globl unlink
    .globl sleep
    .globl uptime

getpid:
    li     a7, SYSCALL_getpid     # 加载系统调用号
    ecall                         # 陷入内核
    ret                           # 从函数返回 (a0 中是内核的返回值)

fork:
    li     a7, SYSCALL_fork
    ecall
    ret

wait:
    li     a7, SYSCALL_wait
    ecall
    ret                           # a0, a1, a2 已经在C调用时被正确设置了

exit:
    li     a7, SYSCALL_exit
    ecall
    ret

exec:
    li     a7, SYSCALL_exec
    ecall
    ret

open:
    li     a7, SYSCALL_open
    ecall
    ret

close:
    li     a7, SYSCALL_close
    ecall
    ret

read:
    li     a7, SYSCALL_read
    ecall
    ret

write:
    li     a7, SYSCALL_write
    ecall
    ret

fstat:
    li     a7, SYSCALL_fstat
    ecall
    ret

mkdir:
    li     a7, SYSCALL_mkdir
    ecall
    ret

sysinfo:
    li     a7, SYSCALL_sysinfo
    ecall
    ret

sem_open:
    li     a7, SYSCALL_sem_open
    ecall
    ret

sem_wait:
    li     a7, SYSCALL_sem_wait
    ecall
    ret

sem_signal:
    li     a7, SYSCALL_sem_signal
    ecall
    ret

sbrk:
    li     a7, SYSCALL_sbrk
    ecall
    ret

fslog_crash:
    li     a7, SYSCALL_fslog_crash
    ecall
    ret

chdir:
    li     a7, SYSCALL_chdir
    ecall
    ret

pipe:
    li     a7, SYSCALL_pipe
    ecall
    ret

link:
    li     a7, SYSCALL_link
    ecall
    ret

unlink:
    li     a7, SYSCALL_unlink
    ecall
    ret

sleep:
    li     a7, SYSCALL_sleep
    ecall
    ret

uptime:
    li     a7, SYSCALL_uptime
    ecall
    ret
