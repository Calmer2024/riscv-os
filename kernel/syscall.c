#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/syscall.h"

#include "../include/console.h"
#include "../include/kalloc.h"
#include "../include/memlayout.h"
#include "../include/param.h"
#include "../include/sem.h"
#include "../include/string.h"
#include "../include/vm.h"

#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

extern volatile uint ticks;

// 系统调用表
static uint64 (*syscalls[])(void) = {
    [SYSCALL_getpid] = syscall_getpid,
    [SYSCALL_fork] = syscall_fork,
    [SYSCALL_wait] = syscall_wait,
    [SYSCALL_exit] = syscall_exit,
    [SYSCALL_exec] = syscall_exec,
    [SYSCALL_open] = syscall_open,
    [SYSCALL_close] = syscall_close,
    [SYSCALL_read] = syscall_read,
    [SYSCALL_write] = syscall_write,
    [SYSCALL_fstat] = syscall_fstat,
    [SYSCALL_sysinfo] = syscall_sysinfo,
    [SYSCALL_mkdir] = syscall_mkdir,
    [SYSCALL_chdir] = syscall_chdir,
    [SYSCALL_sem_open] = syscall_sem_open,
    [SYSCALL_sem_wait] = syscall_sem_wait,
    [SYSCALL_sem_signal] = syscall_sem_signal,
    [SYSCALL_sbrk] = syscall_sbrk,
    [SYSCALL_fslog_crash] = syscall_fslog_crash,
    [SYSCALL_pipe] = syscall_pipe,
    [SYSCALL_link] = syscall_link,
    [SYSCALL_unlink] = syscall_unlink,
    [SYSCALL_sleep] = syscall_sleep,
    [SYSCALL_uptime] = syscall_uptime
};

void syscall(void) {
    struct proc *p = proc_running();
    int num = (int) p->trapframe->a7; // 从 a7 寄存器获取系统调用号
    if (num >= 0 && num < NELEM(syscalls) && syscalls[num]) {
        // 调用对应的处理函数，并把返回值存入 a0
        uint64 ret = syscalls[num]();

        // exec 成功时会在新的 trapframe 上设置 argc/argv，不要覆盖 a0
        if (!(num == SYSCALL_exec && ret == 0)) {
            p->trapframe->a0 = ret;
        }
    } else {
        printf("pid %d: unknown syscall num %d\n", p->pid, num);
        p->trapframe->a0 = -1; // 返回 -1 表示错误
    }
}

// 从陷阱帧中获取第 n 个整数参数
int argint(int n, int *ip) {
    struct proc *p = proc_running();
    switch (n) {
        case 0: *ip = (int) p->trapframe->a0;
            return 0;
        case 1: *ip = (int) p->trapframe->a1;
            return 0;
        case 2: *ip = (int) p->trapframe->a2;
            return 0;
        case 3: *ip = (int) p->trapframe->a3;
            return 0;
        case 4: *ip = (int) p->trapframe->a4;
            return 0;
        case 5: *ip = (int) p->trapframe->a5;
            return 0;
        default: return -1;
    }
}

// 从陷阱帧中获取第 n 个指针参数 (64位地址)
int argaddr(int n, uint64 *ip) {
    struct proc *p = proc_running();
    switch (n) {
        case 0: *ip = p->trapframe->a0;
            return 0;
        case 1: *ip = p->trapframe->a1;
            return 0;
        case 2: *ip = p->trapframe->a2;
            return 0;
        case 3: *ip = p->trapframe->a3;
            return 0;
        case 4: *ip = p->trapframe->a4;
            return 0;
        case 5: *ip = p->trapframe->a5;
            return 0;
        default: return -1;
    }
}

int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max) {
    uint64 n, va0, pa0;
    int got_null = 0;

    while (got_null == 0 && max > 0) {
        va0 = PAGE_DOWN(srcva);
        pa0 = vmem_walk_addr(pagetable, va0);
        if (pa0 == 0)
            return -1;
        n = PAGE_SIZE - (srcva - va0);
        if (n > max)
            n = max;

        char *p = (char *) (pa0 + (srcva - va0));
        while (n > 0) {
            if (*p == '\0') {
                *dst = '\0';
                got_null = 1;
                break;
            } else {
                *dst = *p;
            }
            --n;
            --max;
            p++;
            dst++;
        }

        srcva = va0 + PAGE_SIZE;
    }
    if (got_null) {
        return 0;
    } else {
        return -1;
    }
}

int
fetchstr(uint64 addr, char *buf, int max) {
    struct proc *p = proc_running();
    int ret = copyinstr(p->pagetable, buf, addr, max);
    if (ret < 0)
        return -1;
    return strlen(buf);
}

// 从用户空间获取字符串
int argstr(int n, char *buf, int max) {
    uint64 addr;
    argaddr(n, &addr);
    return fetchstr(addr, buf, max);
}

uint64 syscall_getpid(void) {
    return proc_running()->pid;
}

uint64 proc_fork();

uint64 syscall_fork(void) {
    return proc_fork();
}

uint64 syscall_wait(void) {
    uint64 p;
    argaddr(0, &p);
    return wait(p);
}

uint64 syscall_exit(void) {
    int n;
    argint(0, &n);
    exit(n);
    panic("syscall_exit returned");
    return 0;
}

uint64 exec(char *path, char **argv);

// // 从用户空间 addr 处读取一个 uint64 到 dst
// static int fetchaddr(uint64 addr, uint64 *dst) {
//     struct proc *p = proc_running();
//     if (addr >= p->size || addr + sizeof(uint64) > p->size)
//         return -1;
//     if (vmem_copyin(p->pagetable, (char *)dst, addr, sizeof(uint64)) < 0)
//         return -1;
//     return 0;
// }

// 用户态调用: exec(path, argv)
// a0: path 地址
// a1: argv 数组地址 (char *argv[])
uint64 syscall_exec(void) {
    char path[MAXPATH];
    uint64 uargv; // 用户空间的 argv 数组地址
    char *argv[MAXARG]; // 内核空间的 argv 字符串指针数组
    int i;

    // 1. 获取 path 参数
    if (argstr(0, path, MAXPATH) < 0) {
        return -1;
    }

    // 2. 获取 argv 数组地址
    if (argaddr(1, &uargv) < 0) {
        return -1;
    }
    // 显式检查：argv 数组本身不能是 NULL
    if (uargv == 0) {
        return -1;
    }

    // 3. 将 argv 数组中的字符串全部读入内核
    memset(argv, 0, sizeof(argv));
    for (i = 0; i < MAXARG; i++) {
        uint64 uarg;

        // 从用户空间读取 argv[i] 的值 (这是一个指针)
        // uargv 是数组首地址，uargv + i*8 是第 i 个元素的地址
        if (vmem_copyin(proc_running()->pagetable, (char *) &uarg, uargv + sizeof(uint64) * i, sizeof(uint64)) < 0) {
            goto bad;
        }

        // 如果读到 0 (NULL)，说明参数结束
        if (uarg == 0) {
            argv[i] = 0;
            break;
        }

        // 为字符串分配内核临时内存
        argv[i] = kmem_alloc(); // 这一页哪怕只存个短字符串稍微有点浪费，但最简单 TODO: ？给一个字符串分配一页？
        if (argv[i] == 0) goto bad;

        // 从用户空间 uarg 处读取字符串到内核 argv[i]
        if (fetchstr(uarg, argv[i], PAGE_SIZE) < 0) {
            goto bad;
        }
    }

    // 4. 调用真正的 exec
    int ret = exec(path, argv);

    // exec 成功的话不会返回这里；如果返回了说明失败了
    // 释放刚才分配的 argv 内存
    for (i = 0; i < MAXARG && argv[i] != 0; i++) {
        kmem_free(argv[i]);
    }
    return ret;

bad:
    for (i = 0; i < MAXARG && argv[i] != 0; i++) {
        kmem_free(argv[i]);
    }
    return -1;
}

uint64 syscall_sem_open(void) {
    int init_val;
    argint(0, &init_val);
    return sem_open(init_val);
}

// TODO：其实还应该检查id是否属于该进程
uint64 syscall_sem_wait(void) {
    int sem_id;
    argint(0, &sem_id);
    return sem_wait_id(sem_id);
}

uint64 syscall_sem_signal(void) {
    int sem_id;
    argint(0, &sem_id);
    return sem_signal_id(sem_id);
}

uint64 syscall_sbrk(void) {
    int size;
    argint(0, &size);
    struct proc *p = proc_running();
    int old_sz = (int) p->size;
    if (proc_grow(size) < 0) {
        return -1;
    }
    return old_sz;
}

uint64 syscall_fslog_crash(void) {
    extern int FSLOG_TEST_CRASH;
    argint(0, &FSLOG_TEST_CRASH);
    printf("FSLOG_TEST_CRASH: %d\n", FSLOG_TEST_CRASH);
    return 1;
}

uint64 syscall_sleep(void) {
    int n;
    if (argint(0, &n) < 0 || n < 0)
        return -1;

    uint start;
    start = ticks;
    while (ticks - start < (uint) n) {
        sleep((void *) &ticks);
    }
    return 0;
}

uint64 syscall_uptime(void) {
    uint t;
    t = ticks;
    return t;
}
