#include "../include/types.h"
#include "../include/proc.h"

extern void run_tests(void);

uint64
sys_test(void) {
    printf("\n[Syscall] User requested to run tests...\n");
    run_tests();
    return 0;
}

uint64
sys_exit(void)
{
    int n;
    argint(0, &n);
    printf("[System Call] PID %d called exit with status %d\n", myproc()->pid, n); // <--- 加这行
    kexit(n);
    return 0;  // not reached
}

uint64
sys_getpid(void)
{
    return myproc()->pid;
}

uint64
sys_fork(void)
{
    return kfork();
}

uint64
sys_wait(void)
{
    uint64 p;
    argaddr(0, &p);
    return kwait(p);
}

uint64
sys_kill(void)
{
    int pid;
    argint(0, &pid);
    return kkill(pid);
}
