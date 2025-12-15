#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/syscall.h"
#include "../include/string.h"
#include "../include/vm.h" // 假设 vm.h 提供了 copy_from_user / copy_to_user

// ====================================================================
// 参数获取与校验
// ====================================================================
// 用参数去读用户内存（真正危险的地方）
int
fetchaddr(uint64 addr, uint64 *ip)
{
    struct proc *p = myproc();
    if(addr >= p->sz || addr+sizeof(uint64) > p->sz) // both tests needed, in case of overflow
        return -1;
    if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
        return -1;
    return 0;
}

int
fetchstr(uint64 addr, char *buf, int max)
{
    struct proc *p = myproc();
    if(copyinstr(p->pagetable, buf, addr, max) < 0)
        return -1;
    return strlen(buf);
}
// 从 trapframe 里拿参数
// 从用户寄存器取值
static uint64
argraw(int n)
{
    struct proc *p = myproc();
    switch (n) {
    case 0:
        return p->trapframe->a0;
    case 1:
        return p->trapframe->a1;
    case 2:
        return p->trapframe->a2;
    case 3:
        return p->trapframe->a3;
    case 4:
        return p->trapframe->a4;
    case 5:
        return p->trapframe->a5;
    }
    panic("argraw");
    return -1;
}
// 值解释
void
argint(int n, int *ip)
{
    *ip = argraw(n);
}
// 值解释为地址
void
argaddr(int n, uint64 *ip)
{
    *ip = argraw(n);
}

int
argstr(int n, char *buf, int max)
{
    uint64 addr;
    argaddr(n, &addr);
    return fetchstr(addr, buf, max);
}

// ====================================================================
// 系统调用实现函数
// ====================================================================
// 这些函数在 sysproc.c 中定义
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_wait(void);
extern uint64 sys_kill(void);
extern uint64 sys_getpid(void);
extern uint64 sys_write(void);
extern uint64 sys_test(void);

// --- 系统调用表 ---
static uint64 (*syscalls[])(void) = {
    [SYS_exit]   = sys_exit,
    [SYS_fork]   = sys_fork,
    [SYS_wait]   = sys_wait,
    [SYS_kill]   = sys_kill,
    [SYS_getpid] = sys_getpid,
    [SYS_write]  = sys_write,
    [SYS_test]   = sys_test,
  };

// --- 系统调用分发器 ---
void syscall_dispatch(void) {
    struct proc *p = myproc();
    int num = p->trapframe->a7; // RISC-V 约定：a7 保存系统调用号

    if(num > 0 && num < sizeof(syscalls)/sizeof(syscalls[0]) && syscalls[num]) {
        // 调用处理函数，并将返回值保存回 a0
        p->trapframe->a0 = syscalls[num]();
    } else {
        printf("PID %d: unknown sys call %d\n", p->pid, num);
        p->trapframe->a0 = -1; // 失败返回 -1
    }
}