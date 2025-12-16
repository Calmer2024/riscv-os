#ifndef PROC_H
#define PROC_H
#include "file.h"
#include "riscv.h"
#include "types.h"

// 进程陷入内核态进行调度切换时保存上下文
struct context {
    uint64 ra;
    uint64 sp;
    // 保存调用者保存寄存器，被调用者保存寄存器由调用switch函数的地方进行保存与恢复
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

// 保存用户态的完整现场，用于特权级切换
struct trapframe {
    /*   0 */
    uint64 kernel_pagetable; // 内核根页表
    /*   8 */
    uint64 kernel_sp; // 内核栈
    /*  16 */
    uint64 kernel_trap; // usertrap()函数入口
    /*  24 */
    uint64 epc; // 陷入trap时候的pc值
    /*  32 */
    uint64 kernel_hartid; // saved kernel tp
    /*  40 */
    uint64 ra;
    /*  48 */
    uint64 sp;
    /*  56 */
    uint64 gp;
    /*  64 */
    uint64 tp;
    /*  72 */
    uint64 t0;
    /*  80 */
    uint64 t1;
    /*  88 */
    uint64 t2;
    /*  96 */
    uint64 s0;
    /* 104 */
    uint64 s1;
    /* 112 */
    uint64 a0;
    /* 120 */
    uint64 a1;
    /* 128 */
    uint64 a2;
    /* 136 */
    uint64 a3;
    /* 144 */
    uint64 a4;
    /* 152 */
    uint64 a5;
    /* 160 */
    uint64 a6;
    /* 168 */
    uint64 a7;
    /* 176 */
    uint64 s2;
    /* 184 */
    uint64 s3;
    /* 192 */
    uint64 s4;
    /* 200 */
    uint64 s5;
    /* 208 */
    uint64 s6;
    /* 216 */
    uint64 s7;
    /* 224 */
    uint64 s8;
    /* 232 */
    uint64 s9;
    /* 240 */
    uint64 s10;
    /* 248 */
    uint64 s11;
    /* 256 */
    uint64 t3;
    /* 264 */
    uint64 t4;
    /* 272 */
    uint64 t5;
    /* 280 */
    uint64 t6;
};

enum procstate {
    UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE
};

struct proc {
    enum procstate state;
    int pid;
    struct proc *parent;
    uint64 kstack; // 内核栈栈顶虚拟地址
    pagetable_t pagetable; // 进程页表
    struct trapframe *trapframe; // 陷阱帧的指针
    struct context context; // 内核线程上下文
    uint64 size; // 进程占用的内存大小。假设进程的虚拟地址空间是从0开始一直到sz
    void *sleep_channel; // 进程等待的频道
    int exit_status; // 退出的状态码

    struct file *open_file[NOFILE];  // NOFILE 通常定义为 16
    struct inode *cwd; // 当前工作目录
};

struct cpu {
    struct proc *proc; // 正在运行的进程
    struct context context; // cpu自己的上下文
};

extern struct cpu cpu;

void swtch(struct context *, struct context *);

struct proc *proc_running();

void scheduler(void);

struct proc *kproc_create(void (*proc_func)());

void proc_free_pagetable(pagetable_t pagetable, uint64 size);

void kproc_test(void);

void yield(void);

struct proc *proc_alloc(void);

void proc_userinit();

void exit(int status);

void sleep(void *channel);

void wakeup(void *channel);

int wait(uint64 status_va);

int proc_grow(int size);

#endif //PROC_H
