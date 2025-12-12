#include "../include/types.h"
#include "../include/proc.h"
#include "../include/pmm.h"
#include "../include/memlayout.h"
#include "../include/string.h"
#include "../include/spinlock.h"
#include "../include/riscv.h"
#include "../include/vm.h"
#include "../include/printf.h"
#include "../include/stddef.h"

struct proc proc[NPROC];    // 全局进程表，NPROC是64
struct cpu cpus[1];
static int nextpid = 1;     // 下一个可用的PID

extern void proc_trampoline(void);
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

// 进程第一次被调度时的“蹦床”函数
// 模仿 xv6 的 forkret
void forkret(void) {
    struct proc *p = myproc();
    printf("DEBUG: forkret entry for PID %d\n", p->pid);

    // 关键点：调度器在切换到我们可以运行之前，获取了 p->lock。
    // 现在我们开始运行了，必须释放它，否则无法进行后续操作，
    // 也无法被其他 CPU (如果有多核) 再次调度。
    release(&p->lock);

    // 对于内核线程，真正的入口地址保存在 s0 寄存器中
    // (因为 s0 是 callee-saved，swtch 会恢复它)
    if (p->context.s0 != 0) {
        void (*fn)(void) = (void(*)(void))p->context.s0;
        fn(); // 跳转到 simple_task
    }

    // 如果任务函数返回了，则退出线程
    kthread_exit(0);
}

// --- 进程生命周期管理 ---
// 进程分配
struct proc* alloc_process(void) {
    struct proc *p;

    // 1. 寻找空闲进程位
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
    p->state = SLEEPING; // 暂时设为占用

    // 2. 设置内核栈
    // [重大简化]：我们不再 alloc_page，也不再计算 sp。
    // p->kstack 已经在 kvminit -> proc_mapstacks 里设置好了虚拟地址！
    // 我们只需要检查一下它是不是有效。
    if(p->kstack == 0) {
        panic("alloc_process: no kstack");
    }

    // 3. 设置 Trapframe
    //    暂时不需要专门分配页，因为内核线程不需要用户态 Trapframe。
    //    但在完整 OS 中，这里会 alloc_page 并赋值给 p->trapframe。

    // 4. 初始化上下文
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64)forkret;

    // [关键]：栈指针 sp 指向栈顶
    // 因为 p->kstack 指向的是栈底（低地址），所以栈顶是 p->kstack + PGSIZE
    p->context.sp = p->kstack + PGSIZE;

    return p;
}

// 释放进程资源
void free_process(struct proc *p) {
    // p 必须被锁住

    // 释放内核栈
    // if(p->kstack)
    //     free_page((void*)p->kstack);
    // p->kstack = 0;

    // 释放页表
    if(p->pagetable && p->pagetable != kernel_pagetable)
        destroy_pagetable(p->pagetable);
    // if(p->pagetable)
    //     destroy_pagetable(p->pagetable); // 假设 vm.h 有
    p->pagetable = 0;

    p->pid = 0;
    p->parent = 0;
    p->trapframe = 0;
    p->chan = 0;
    p->state = UNUSED;

    release(&p->lock); // 释放锁
}

// 内核态进程创建
int create_kthread(void (*entry)(void)) {
    struct proc *p = alloc_process();
    if(p == 0) return -1;

    p->pagetable = kernel_pagetable;

    // 我们不再直接设置 ra = entry
    // 而是把 entry 藏在 s0 寄存器里
    p->context.s0 = (uint64)entry;

    // 注意：alloc_process 已经把 p->context.ra 设置为 forkret 了
    // 所以不需要在这里动 ra

    p->parent = myproc();

    // 针对 main 创建第一个进程时的特殊处理
    // 如果当前系统刚启动，myproc() 可能返回 0
    if (p->parent == 0) {
        // 允许没有父进程 (对于 PID 1)
    }

    p->state = RUNNABLE;
    release(&p->lock);
    return p->pid;
}

// 内核态进程退出
void kthread_exit(int status) {
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
int kthread_wait(int *status) {
    struct proc *p = myproc();

    // 安全检查
    if (p == NULL) {
        panic("kthread_wait: not called from a process");
    }

    for(;;) {
        int have_kids = 0;
        struct proc *child;

        for(child = proc; child < &proc[NPROC]; child++) {
            // if (child->state != UNUSED) {
            //     printf("DEBUG: Scanning PID %d, state %d, parent %p (my addr %p)\n",
            //     child->pid, child->state, child->parent, p);
            // }

            // 如果不是孩子跳过
            if(child->parent != p) {
                continue;
            }

            acquire(&child->lock);

            have_kids = 1;
            if(child->state == ZOMBIE) {
                printf("DEBUG: Found ZOMBIE child PID %d!\n", child->pid);
                // 找到了！
                if(status != 0) {
                    *status = child->exit_status; // 拷贝退出码
                }

                int child_pid = child->pid;
                free_process(child); // 回收 (这个函数会释放 child->lock)
                return child_pid;
            }

            release(&child->lock);
        }

        if(!have_kids) {
            printf("DEBUG: PID %d has NO kids! Returning -1.\n", p->pid);
            return -1; // 没有子进程
        }

        // printf("DEBUG: PID %d sleeping waiting for children...\n", p->pid);
        // 有子进程，但它们还在运行
        acquire(&p->lock);
        sleep(p, &p->lock); // 休眠在自己身上
        release(&p->lock);
        printf("DEBUG: PID %d woke up!\n", p->pid);
    }
}

// 调度器
void scheduler(void) {
    struct proc *p;
    struct cpu *c = mycpu();

    c->proc = 0;
    for(;;) {
        // 开启中断 (允许时钟中断等)
        intr_on();

        // 循环查找可运行进程 (Round-Robin)
        for(p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if(p->state == RUNNABLE) {
                // 找到！切换到它
                p->state = RUNNING;
                c->proc = p;

                // 切换！
                swtch(&c->context, &p->context);

                // 切换回来了
                c->proc = 0;
            }
            release(&p->lock);
        }
    }
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



