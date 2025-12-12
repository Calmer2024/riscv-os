// 作用: 定义进程、上下文和陷阱帧，这是保存CPU状态的“容器”。
#ifndef __PROC_H__
#define __PROC_H__

#include "types.h"
#include "spinlock.h"
#include "vm.h"

#define NPROC 64 // 最大进程数

// 上下文结构体 (用于 swtch)
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

// 核心进程结构体
struct proc {
    struct spinlock lock;     // 保护进程数据的自旋锁
    enum procstate state;     // 进程状态(UNUSED/USED/RUNNABLE/RUNNING)
    int pid;                  // 进程ID
    struct proc *parent;      // 父进程

    uint64 kstack;            // 此进程的内核栈 (虚拟地址)
    pagetable_t pagetable;    // 进程的页表
    struct trapframe *trapframe; // 用户陷阱帧 (位于kstack顶部)
    struct context context;   // 上下文，用于 swtch 切换

    uint64 sz;                // 进程内存大小 (bytes)
    int exit_status;          // 退出状态码 (供 wait() 读取)

    void *chan;               // 如果在 SLEEPING，休眠在哪个通道上
    char name[16];            // 进程名（调试用）
};

extern struct cpu cpus[1]; // 只有一个核心
extern struct proc proc[NPROC];

// 陷阱帧：就是一个在内存中创建的数据结构，它的唯一目的就是完整保存当陷阱发生时，CPU 的完整执行状态（上下文）
// 寄存器上下文，由kernelvec.S保存
// 32个通用寄存器 (x0-x31)
// sepc: 发生陷阱时指令的地址
// sstatus: 状态寄存器
struct trapframe {
    uint64 kernel_satp;   // 内核页表
    uint64 kernel_sp;     // 内核栈顶
    uint64 kernel_trap;   // kerneltrap()函数的地址
    uint64 epc;           // sepc寄存器
    uint64 kernel_hartid; // cpuid，我们单核不需要
    // 相关寄存器，存档程序计算时的临时数据、参数、返回值等
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
void proc_init_main(void);
int create_kthread(void (*entry)(void));
void kthread_exit(int status);
int kthread_wait(int *status);
void scheduler(void) __attribute__((noreturn));
void swtch(struct context *old, struct context *new);
void yield(void);
void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);

// CPU相关
struct cpu* mycpu(void);
struct proc* myproc(void);

void usertrapret(void);

#endif