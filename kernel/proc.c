#include "../include/types.h"
#include "../include/proc.h"
#include "../include/memlayout.h"
#include "../include/string.h"
#include "../include/spinlock.h"
#include "../include/vm.h"
#include "../include/printf.h"
#include "../include/stddef.h"
#include "../include/riscv.h"
#include "../include/pmm.h"
#include "../include/trap.h"

struct proc proc[NPROC];    // 全局进程表，NPROC是64
struct cpu cpus[1];
static int nextpid = 1;     // 下一个可用的PID

extern void trampoline(void);
extern void uservec(void);
extern void userret(void);
extern pagetable_t kernel_pagetable;


// 辅助函数
// 获取当前CPU
struct cpu* mycpu(void) {
    return &cpus[0]; // 单核
}

// 获取当前进程
struct proc* myproc(void) {
    push_off();
    struct proc *p = mycpu()->proc;
    pop_off();
    return p;
}

// 进程入口（用于 swtch 后的 trampoline）
// (我们将来会为 用户进程 修复这个函数)
void proc_trampoline(void) {
    // ...
    // release(&myproc()->lock);
    // ...
}

// 初始化进程表
void proc_init(void) {
    for(int i = 0; i < NPROC; i++) {
        initlock(&proc[i].lock, "proc");
    }
}

// ====================================================================
// 虚拟内存辅助函数
// ====================================================================
// 为给定进程创建一个用户页表
// 仅映射 Trampoline 和 Trapframe，不映射用户程序内存(稍后由 uvminit 或 exec 完成)
pagetable_t proc_pagetable(struct proc *p) {
    pagetable_t pagetable;

    // 1. 创建空页表
    pagetable = uvmcreate();
    if(pagetable == 0) return 0;

    // 2. 映射 Trampoline (在最高地址 MAXVA-PAGE_SIZE)
    //    这是内核和用户共享的代码页，必须有 execute 权限
    if(mappages(pagetable, TRAMPOLINE, PAGE_SIZE,
                (uint64)trampoline, PTE_R | PTE_X) < 0) {
        uvmfree(pagetable, 0);
        return 0;
                }

    // 3. 映射 Trapframe (在 TRAMPOLINE 下方)
    //    映射到该进程独有的 p->trapframe 物理地址
    if(mappages(pagetable, TRAPFRAME, PAGE_SIZE,
                (uint64)(p->trapframe), PTE_R | PTE_W) < 0) {
        uvmunmap(pagetable, TRAMPOLINE, 1, 0);
        uvmfree(pagetable, 0);
        return 0;
                }

    return pagetable;
}

// 释放进程的用户页表
// 释放页表本身，并解除 Trampoline/Trapframe 的映射
void proc_freepagetable(pagetable_t pagetable, uint64 sz) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0); // 解除 Trampoline
    uvmunmap(pagetable, TRAPFRAME, 1, 0);  // 解除 Trapframe
    uvmfree(pagetable, sz);                // 释放用户内存和页表页
}

// ====================================================================
// 内核返回用户空间
// ====================================================================
void usertrapret(void) {
    struct proc *p = myproc();

    // 关闭中断，因为我们要修改 stvec 和 sstatus
    intr_off();

    // 1. 设置陷阱向量 (Trap Vector)
    //    当用户空间发生 Trap 时，CPU 指向 trampoline 中的 uservec
    //    注意：stvec 存储的是虚拟地址
    uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
    w_stvec(trampoline_uservec);

    // 2. 填充 Trapframe 中内核所需的信息
    //    这样 uservec 才能在下次 trap 时恢复内核环境
    p->trapframe->kernel_satp = r_satp();         // 内核页表
    p->trapframe->kernel_sp = p->kstack + PAGE_SIZE; // 内核栈顶
    p->trapframe->kernel_trap = (uint64)usertrap; // 内核 C 处理函数
    p->trapframe->kernel_hartid = r_tp();         // CPU ID

    // 3. 配置 SSTATUS 寄存器
    //    SPP = 0 (Previous mode was User)
    //    SPIE = 1 (Enable interrupts in User mode)
    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE; // enable interrupts in user mode
    w_sstatus(x);

    // 4. 设置 SEPC (Exception Program Counter)
    //    sret 之后 PC 将跳转到这里
    w_sepc(p->trapframe->epc);

    // 5. 准备 userret 的参数
    //    fn: userret 在 trampoline 页中的虚拟地址
    uint64 fn = TRAMPOLINE + (userret - trampoline);
    uint64 satp = MAKE_SATP(p->pagetable);

    printf("TRAMPOLINE=%p trampoline=%p userret=%p fn=%p off=%p\n",
       TRAMPOLINE, (uint64)trampoline, (uint64)userret, fn,
       (uint64)userret - (uint64)trampoline);

    // 6. 跳转到 trampoline.S 中的 userret
    //    (uint64, uint64) 对应汇编中的 (a0, a1)
    ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);
}

// ====================================================================
// 进程调度函数
// ====================================================================
// 进程第一次被调度时的“蹦床”函数
void forkret(void) {
    static int first = 1;

    // 释放调度器锁 (myproc()->lock)
    release(&myproc()->lock);

    if (first) {
        // File system initialization etc.
        printf("forkret: first process started\n");
        first = 0;
    }

    // 从内核态返回用户态
    usertrapret();
}

// 进程分配
struct proc* alloc_process(void) {
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if(p->state == UNUSED) {
            goto found;
        } else {
            release(&p->lock);
        }
    }
    return 0;

    found:
        p->pid = nextpid++;
        p->state = SLEEPING;

        // 1. 分配 Trapframe 物理页
        if((p->trapframe = (struct trapframe *)alloc_page()) == 0){
            release(&p->lock);
            return 0;
        }
        memset(p->trapframe, 0, PAGE_SIZE);

        // 2. 创建用户页表
        p->pagetable = proc_pagetable(p);
        if(p->pagetable == 0){
            free_page((void*)p->trapframe); // 释放刚才申请的 tf
            release(&p->lock);
            return 0;
        }

        // 3. 初始化 Context (用于 swtch)
        memset(&p->context, 0, sizeof(p->context));
        // [重要] 这里 ra 设置为 forkret
        // 当该进程被 scheduler 调度并执行 swtch 后，会跳转到 forkret
        // forkret 随后会调用 usertrapret 进入用户空间
        p->context.ra = (uint64)forkret;
        p->context.sp = p->kstack + PAGE_SIZE; // 内核栈

        return p;
}

// 释放进程资源
void free_process(struct proc *p) {
    // p 必须被锁住
    if(p->trapframe)
        free_page((void*)p->trapframe);
    p->trapframe = 0;

    if(p->pagetable)
        proc_freepagetable(p->pagetable, p->sz);
    p->pagetable = 0;

    p->sz = 0;
    p->pid = 0;
    p->parent = 0;
    p->chan = 0;
    p->state = UNUSED;

    release(&p->lock);
}

// 调度器 (保持不变，但使用了 wfi 优化)
void scheduler(void) {
    struct proc *p;
    struct cpu *c = mycpu();

    c->proc = 0;
    for(;;) {
        intr_on();
        asm volatile("wfi"); // 节能

        for(p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if(p->state == RUNNABLE) {
                p->state = RUNNING;
                c->proc = p;

                // 切换到进程的内核上下文
                swtch(&c->context, &p->context);

                c->proc = 0;
            }
            release(&p->lock);
        }
    }
}

uchar initcode[] = {
    0x13, 0x05, 0x10, 0x00, // li a0, 1 (SYS_exit, 假设放在 a0 传参? 不, 通常 a7 传系统调用号)
                            // 让我们用标准 RISC-V 调用约定: a7=sys_num, a0=arg0

    // li a7, 1 (SYS_exit) -> 0x00100893
    0x93, 0x08, 0x10, 0x00,

    // li a0, 0 (status)   -> 0x00000513
    0x13, 0x05, 0x00, 0x00,

    // ecall               -> 0x00000073
    0x73, 0x00, 0x00, 0x00,

    // loop: j loop        -> 0x0000006f
    0x6f, 0x00, 0x00, 0x00
};

void userinit(void) {
    struct proc *p;

    p = alloc_process();
    if(p == 0) panic("userinit: alloc failed");

    // 初始化用户页表并拷贝 initcode
    uvminit(p->pagetable, initcode, sizeof(initcode));
    p->sz = PAGE_SIZE; // 大小设为一页

    // 设置 Trapframe
    // epc 指向虚拟地址 0 (initcode 被加载到了地址 0)
    p->trapframe->epc = 0;
    // sp 指向该页的顶部 (4096)
    p->trapframe->sp = PAGE_SIZE;

    // 设置进程名
    safestrcpy(p->name, "initcode", sizeof(p->name));

    p->state = RUNNABLE;

    release(&p->lock);
    printf("userinit: created PID %d\n", p->pid);
}

// 内核态进程创建
// int create_kthread(void (*entry)(void)) {
//     struct proc *p = alloc_process();
//     if(p == 0) return -1;
//
//     p->pagetable = kernel_pagetable;
//
//     // 我们不再直接设置 ra = entry
//     // 而是把 entry 藏在 s0 寄存器里
//     p->context.s0 = (uint64)entry;
//
//     // 注意：alloc_process 已经把 p->context.ra 设置为 forkret 了
//     // 所以不需要在这里动 ra
//
//     p->parent = myproc();
//
//     // 针对 main 创建第一个进程时的特殊处理
//     // 如果当前系统刚启动，myproc() 可能返回 0
//     if (p->parent == 0) {
//         // 允许没有父进程 (对于 PID 1)
//     }
//
//     p->state = RUNNABLE;
//     release(&p->lock);
//     return p->pid;
// }

// 内核态进程退出
void kexit(int status) {
    struct proc *p = myproc();

    // (不做文件清理等，因为内核线程没有)

    acquire(&p->lock);

    p->state = ZOMBIE;
    p->exit_status = status;

    // 唤醒父进程 (如果它在 kthread_wait)
    if(p->parent) {
        wakeup(p->parent);
    }

    // 永久切换回调度器
    swtch(&p->context, &mycpu()->context);

    // 不会返回
}

// 内核态进程等待
int kwait(uint64 addr) {
    struct proc *p = myproc();

    // 安全检查
    if (p == NULL) {
        panic("kwait: not called from a process");
    }

    acquire(&p->lock); // 获取当前进程锁（如果你的sleep机制需要）

    for(;;) {
        int have_kids = 0;
        struct proc *child;

        // 遍历进程表
        for(child = proc; child < &proc[NPROC]; child++) {
            if(child->parent != p) {
                continue;
            }

            // 必须先获取子进程的锁
            acquire(&child->lock);

            have_kids = 1;
            if(child->state == ZOMBIE) {
                // 找到了僵尸子进程
                int child_pid = child->pid;

                // --- 核心修改开始 ---
                // 如果用户提供了有效的地址 (addr != 0)，我们将退出码 copyout 到用户空间
                if(addr != 0 && copyout(p->pagetable, addr, (char *)&child->exit_status, sizeof(child->exit_status)) < 0) {
                    release(&child->lock);
                    release(&p->lock);
                    return -1; // 拷贝失败（通常是因为用户给了个非法地址）
                }
                // --- 核心修改结束 ---

                free_process(child); // 回收子进程资源
                release(&child->lock); // 释放子进程锁

                // 在返回前释放当前进程锁（视你的 sleep 实现细节而定，通常在这里释放）
                // 假设你在进入循环前没有持锁，或者依靠 sleep 释放锁，这里需要注意锁的配对
                release(&p->lock);

                return child_pid;
            }
            release(&child->lock);
        }

        // 如果没有子进程，立即返回错误
        if(!have_kids) {
            release(&p->lock);
            return -1;
        }

        // 有子进程但都在运行，睡眠等待
        // sleep 会原子地释放 p->lock 并进入睡眠
        // 当醒来时，它会重新获取 p->lock
        sleep(p, &p->lock);
    }
}

// 创建当前进程的副本（子进程）
int kfork(void) {
    int i, pid;
    struct proc *np;
    struct proc *p = myproc();

    // 1. 分配新进程
    // 注意：alloc_process 返回时已经持有了 np->lock
    if ((np = alloc_process()) == 0) {
        return -1;
    }

    // 2. 复制用户内存空间 (页表 + 物理页)
    // 使用我们在 vm.c 中实现的 uvmcopy
    if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) {
        free_process(np); // 释放进程 (free_process 会释放 np->lock)
        return -1;
    }
    np->sz = p->sz;

    // 3. 复制 Trapframe
    // 子进程必须拥有和父进程完全一样的寄存器状态
    *(np->trapframe) = *(p->trapframe);

    // 4. 设置子进程的返回值 (fork 在子进程返回 0)
    np->trapframe->a0 = 0;

    // 5. 复制打开的文件描述符 (暂时略过)
    // 如果你实现了文件系统，这里需要增加文件引用计数
    // for(i = 0; i < NOFILE; i++)
    //     if(p->ofile[i])
    //         np->ofile[i] = filedup(p->ofile[i]);
    // np->cwd = idup(p->cwd);

    // 6. 复制进程名
    safestrcpy(np->name, p->name, sizeof(p->name));

    // 7. 设置父子关系
    np->parent = p;

    // 获取 PID 用于返回
    pid = np->pid;

    // 8. 设置子进程状态为 RUNNABLE
    np->state = RUNNABLE;

    // 9. 释放锁，子进程现在可以被调度器调度了
    release(&np->lock);

    // 返回子进程 PID
    return pid;
}

// kernel/proc.c

// 杀死指定 PID 的进程
int kkill(int pid) {
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++){
        acquire(&p->lock);
        if(p->pid == pid){
            // 1. 设置杀死标志
            p->killed = 1;

            // 2. 如果进程在休眠，唤醒它
            // 这样它醒来后会检查 p->killed 并退出
            if(p->state == SLEEPING){
                // 这是一个稍微粗暴的唤醒，只要状态变了，调度器就会跑它
                p->state = RUNNABLE;
            }

            release(&p->lock);
            return 0; // 成功
        }
        release(&p->lock);
    }
    return -1; // 未找到 PID
}

// 放弃 CPU (用于时钟中断)
void yield(void) {
    struct proc *p = myproc();
    // 添加安全检查
    // 防止在 main 注册为进程前发生时钟中断
    if(p != NULL) {
        acquire(&p->lock);
        p->state = RUNNABLE;
        swtch(&p->context, &mycpu()->context);
        release(&p->lock);
    }
}

void
sleep(void *chan, struct spinlock *lk)
{
    struct proc *p = myproc();

    // 添加安全检查
    if(p == NULL) {
        panic("sleep: not a process");
    }

    // 必须持有锁才能调用sleep
    if(lk != &p->lock){
        acquire(&p->lock);
        release(lk);
    }

    // 4. 设置休眠状态
    p->chan = chan;
    p->state = SLEEPING;

    // 5. 切换回调度器
    //    swtch 会在 p->lock 仍然持有的情况下发生
    swtch(&p->context, &mycpu()->context);

    // --- 唤醒后 ---
    // 6. 进程从这里恢复执行

    // 7. 清理休眠通道
    p->chan = 0;

    if(lk != &p->lock){
        release(&p->lock);
        acquire(lk);
    }
}

//
// 唤醒所有休眠在 chan 上的进程
//
void
wakeup(void *chan)
{
    struct proc *p;
    struct proc *me = myproc();

    // 遍历所有进程
    for(p = proc; p < &proc[NPROC]; p++) {
        // 先检查是否当前进程持有锁，避免自己获取自己锁导致死锁
        if(p != me) {
            acquire(&p->lock);
            if(p->state == SLEEPING && p->chan == chan) {
                p->chan = 0;
                p->state = RUNNABLE;
            }
            release(&p->lock);
        }
    }
}



