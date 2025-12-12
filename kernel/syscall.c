#include "types.h"
#include "riscv.h"
#include "proc.h"
#include "syscall.h"
#include "vm.h" // 假设 vm.h 提供了 copy_from_user / copy_to_user

// --- 声明所有系统调用实现函数 ---
// (这些函数在 sysproc.c 中定义)
extern int sys_fork(void);
extern int sys_exit(void);
extern int sys_wait(void);
extern int sys_kill(void);
extern int sys_getpid(void);
extern int sys_open(void);
extern int sys_close(void);
extern int sys_read(void);
extern int sys_write(void);
extern int sys_sbrk(void);

// --- 系统调用表 ---
static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_kill]    sys_kill,  // (暂未实现)
[SYS_getpid]  sys_getpid,
[SYS_open]    sys_open,  // (暂未实现)
[SYS_close]   sys_close, // (暂未实现)
[SYS_read]    sys_read,  // (暂未实现)
[SYS_write]   sys_write,
[SYS_sbrk]    sys_sbrk,  // (暂未实现)
};

// --- 安全性：参数提取辅助函数 ---
// (响应你的 "get_syscall_arg")

// 从 trapframe 中获取第n个 32位 整数参数
int fetch_arg_int(int n, int *ip) {
    struct proc *p = myproc();
    switch (n) {
        case 0: *ip = p->trapframe->a0; return 0;
        case 1: *ip = p->trapframe->a1; return 0;
        case 2: *ip = p->trapframe->a2; return 0;
        case 3: *ip = p->trapframe->a3; return 0;
        case 4: *ip = p->trapframe->a4; return 0;
        case 5: *ip = p->trapframe->a5; return 0;
    }
    return -1;
}

// 从 trapframe 中获取第n个 64位 地址参数
int fetch_arg_addr(int n, uint64 *ip) {
    struct proc *p = myproc();
    switch (n) {
        case 0: *ip = p->trapframe->a0; return 0;
        case 1: *ip = p->trapframe->a1; return 0;
        case 2: *ip = p->trapframe->a2; return 0;
        case 3: *ip = p->trapframe->a3; return 0;
        case 4: *ip = p->trapframe->a4; return 0;
        case 5: *ip = p->trapframe->a5; return 0;
    }
    return -1;
}

// (响应你的 "get_user_string")
// 从用户空间拷贝一个以 NUL 结尾的字符串到内核空间
// 必须检查指针的有效性！
int fetch_str(uint64 user_addr, char *kernel_buf, int max) {
    // 假设: 你们的 vm.c 提供了 copy_from_user
    // int copy_from_user(void *kernel_dst, uint64 user_src, uint64 len);
    // 它会在失败时返回 -1

    // (这是一个简化的实现，真正的实现需要逐字节拷贝直到 NUL)
    if(copy_from_user(kernel_buf, user_addr, max) < 0) {
        return -1;
    }
    // 确保它以 NUL 结尾
    kernel_buf[max-1] = '\0';
    return 0;
}


// --- 系统调用分发器 ---
// (响应你的 "syscall_dispatch")
// (这个函数由 kernel/trap.c 中的 usertrap() 调用)
void syscall_dispatch(void) {
    int num;
    struct proc *p = myproc();

    // 从 trapframe->a7 获取系统调用号
    num = p->trapframe->a7;

    if(num > 0 && num < (sizeof(syscalls)/sizeof(syscalls[0])) && syscalls[num]) {
        // 调用对应的系统调用
        int ret = syscalls[num]();

        // 将返回值存入 trapframe->a0
        p->trapframe->a0 = ret;
    } else {
        printf("pid %d: unknown syscall num %d\n", p->pid, num);
        p->trapframe->a0 = -1; // 错误
    }
}