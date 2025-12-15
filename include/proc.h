#ifndef __PROC_H__
#define __PROC_H__

#include "types.h"
#include "spinlock.h"
#include "vm.h"

#define NPROC 64 // 最大进程数

// 上下文结构体
// 用于 swtch
// 仅保存被调用者寄存器（Callee-saved）
struct context {
    uint64 ra;
    uint64 sp;
    // Callee-saved
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

// CPU核心状态
struct cpu {
    struct proc *proc; // 当前运行的进程
    struct context context; // 调度器上下文（用于swtch）
    int noff;            // 关中断嵌套层数
    int intena;          // 在关中断前，中断是否开启
    uint64 trapstack;
};

// 进程状态
enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// 进程结构体
struct proc {
    struct spinlock lock;     // 保护进程数据的自旋锁
    enum procstate state;     // 进程状态
    int pid;                  // 进程ID
    struct proc *parent;      // 父进程

    uint64 kstack;            // 此进程的内核栈 (虚拟地址)
    pagetable_t pagetable;    // [NEW] 进程的用户页表

    struct trapframe *trapframe; // [MODIFIED] 用户陷阱帧
    // 注意：trapframe 现在指向一个独立的物理页，
    // 在用户空间，它被映射到固定虚拟地址 TRAPFRAME

    struct context context;   // 内核上下文 (用于 swtch 切换)

    uint64 sz;                // 进程内存大小 (用户堆大小)
    int exit_status;          // 退出状态码 (供 wait() 读取)

    int killed;

    void *chan;               // 如果在 SLEEPING，休眠在哪个通道上
    char name[16];            // 进程名 (调试)
};

extern struct cpu cpus[1]; // 只有一个核心
extern struct proc proc[NPROC];

// 陷阱帧：由 uservec.S (trampoline) 保存
// 严格对应 struct trapframe 的内存布局
struct trapframe {
    uint64 kernel_satp;   // 内核页表
    uint64 kernel_sp;     // 内核栈顶
    uint64 kernel_trap;   // usertrap()函数的地址
    uint64 epc;           // sepc寄存器 (用户PC)
    uint64 kernel_hartid; // cpuid
    uint64 ra;
    uint64 sp;
    uint64 gp;
    uint64 tp;
    uint64 t0;
    uint64 t1;
    uint64 t2;
    uint64 s0;
    uint64 s1;
    uint64 a0;
    uint64 a1;
    uint64 a2;
    uint64 a3;
    uint64 a4;
    uint64 a5;
    uint64 a6;
    uint64 a7;
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
    uint64 t3;
    uint64 t4;
    uint64 t5;
    uint64 t6;
};

// 进程管理函数原型
void proc_init(void);
struct proc* alloc_process(void);
void free_process(struct proc *p);
int kwait(uint64 addr);
void kexit(int status);

// 用户进程支持函数
pagetable_t proc_pagetable(struct proc *p);
void proc_freepagetable(pagetable_t pagetable, uint64 sz);
void userinit(void);
int fork(void);
// ------------------------------

void scheduler(void) __attribute__((noreturn));
void swtch(struct context *old, struct context *new);
void yield(void);
void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);

// CPU相关
struct cpu* mycpu(void);
struct proc* myproc(void);

// 陷阱返回
void usertrapret(void);

#endif