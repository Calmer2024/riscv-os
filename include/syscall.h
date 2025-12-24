#ifndef RISCV_OS_SYSCALL_H
#define RISCV_OS_SYSCALL_H

// 系统调用号
#define SYSCALL_getpid 0
#define SYSCALL_fork 1
#define SYSCALL_wait 2
#define SYSCALL_exit 3
#define SYSCALL_exec 4
#define SYSCALL_open 5
#define SYSCALL_close 6
#define SYSCALL_write 7
#define SYSCALL_read 8
#define SYSCALL_mkdir 9
#define SYSCALL_fstat 10
#define SYSCALL_sysinfo 11
#define SYSCALL_sem_open 12
#define SYSCALL_sem_wait 13
#define SYSCALL_sem_signal 14
#define SYSCALL_sbrk 15
#define SYSCALL_chdir 16
#define SYSCALL_pipe 17
#define SYSCALL_link 18
#define SYSCALL_unlink 19
#define SYSCALL_sleep 20
#define SYSCALL_uptime 21
#define SYSCALL_fslog_crash 100

#ifndef __ASSEMBLER__

void syscall(void);

int argint(int n, int *ip);

int argaddr(int n, uint64 *ip);

int argstr(int n, char *buf, int max);

// 声明所有系统调用函数
uint64 syscall_getpid(void);

uint64 syscall_fork(void);

uint64 syscall_wait(void);

uint64 syscall_exit(void);

uint64 syscall_exec(void);

uint64 syscall_open(void);

uint64 syscall_close(void);

uint64 syscall_read(void);

uint64 syscall_write(void);

uint64 syscall_fstat(void);

uint64 syscall_sysinfo(void);

uint64 syscall_mkdir(void);

uint64 syscall_pipe(void);

uint64 syscall_link(void);

uint64 syscall_unlink(void);

uint64 syscall_sleep(void);

uint64 syscall_uptime(void);

uint64 syscall_sem_open(void);

uint64 syscall_sem_wait(void);

uint64 syscall_sem_signal(void);

uint64 syscall_sbrk(void);

uint64 syscall_chdir(void);

uint64 syscall_fslog_crash(void);


#endif // __ASSEMBLER__

#endif //RISCV_OS_SYSCALL_H
