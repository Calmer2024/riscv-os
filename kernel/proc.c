#include "../include/proc.h"
#include "../include/kalloc.h"
#include "../include/memlayout.h"
#include "../include/param.h"
#include "../include/printf.h"
#include "../include/string.h"
#include "../include/trap.h"
#include "../include/vm.h"

struct cpu cpu;

extern pagetable_t kernel_root_pagetable;


// 进程控制块数组，采用连续内存分配
struct proc procs[MAX_PROCESS];
// pid，自增
int nextpid = 1;
struct proc *initproc;

extern char trampoline[]; // trampoline.S

void proc_init() {
    for (int i = 0; i < MAX_PROCESS; i++) {
        procs[i].state = UNUSED;
        procs[i].kstack = KERNEL_STACK(i);
    }
}

void proc_userinit() {
    // 声明 Makefile 为我们准备好的符号
    extern char initcode_start[];
    extern char initcode_end[];

    // printf("proc_userinit: creating first process\n");

    // 1. 分配一个新的进程
    struct proc *p = proc_alloc();

    // 2. 为 initcode 分配一页物理内存，并映射到用户页表的 0x0 地址
    char *pa = kmem_alloc();
    if (pa == 0)
        panic("proc_userinit: kalloc");

    // 权限必须是 U(用户), X(执行), R(读取)
    vmem_map_pagetable(p->pagetable, 0, (uint64) pa, PTE_X | PTE_R | PTE_U);

    // 3. 把 initcode 的内容复制到那页物理内存中
    memmove(pa, initcode_start, (uint64) (initcode_end - initcode_start));

    // 4. 为第一个进程分配一页作为用户栈
    pa = kmem_alloc();
    if (pa == 0)
        panic("proc_userinit: kalloc stack");

    // 用户栈通常放在高地址，但为了简单，我们可以先放在 PAGE_SIZE 的位置
    // 我们把它映射到虚拟地址 PAGE_SIZE (0x1000)
    vmem_map_pagetable(p->pagetable, PAGE_SIZE, (uint64) pa, PTE_W | PTE_R | PTE_U);

    // 5. 关键：设置 trapframe，为第一次返回用户态做准备
    p->trapframe->epc = 0; // 用户代码从地址 0 开始执行
    p->trapframe->sp = PAGE_SIZE * 2; // 用户栈顶在 0x2000

    // 6. 让它“活”过来
    p->state = RUNNABLE;
    p->size = 2 * PAGE_SIZE;

    // 设置cwd为根目录
    p->cwd = fs_namei("/");

    initproc = p;

    printf("proc_userinit: process created, pid %d.\n", p->pid);
}

// 获取当前正在执行的进程
struct proc *proc_running() {
    return cpu.proc;
}

static int proc_allocpid(void) {
    return nextpid++;
}


void proc_free_pagetable(pagetable_t pagetable, uint64 size) {
    vmem_unmap_pagetable(pagetable,TRAMPOLINE, 0);
    vmem_unmap_pagetable(pagetable,TRAPFRAME, 0);
    uint64 va;
    for (va = 0; va < size; va += PAGE_SIZE) {
        // 调用 unmap 并设置 do_free=1，释放物理页
        // vmem_unmap_pagetable 会处理那些未映射的 va (返回-1)，所以我们不用检查
        vmem_unmap_pagetable(pagetable, va, 1);
    }
    // 释放栈区，目前只有一页
    vmem_unmap_pagetable(pagetable,USER_STACK_VA, 1);

    // 3. 释放页表目录页 (非叶子)
    //    因为所有叶子都已被解除，这个函数不会 panic
    vmem_free_pagetable(pagetable);
}

void proc_free(struct proc *p) {
    if (p->trapframe) {
        kmem_free(p->trapframe);
    }
    // 释放内核栈
    vmem_unmap_pagetable(kernel_root_pagetable, p->kstack, 1);
    p->trapframe = 0;
    proc_free_pagetable(p->pagetable, p->size);
    p->pagetable = 0;
    p->state = UNUSED;
    p->pid = 0;
    p->parent = 0;
    p->size = 0;
    p->sleep_channel = 0;
    p->exit_status = 0;
    memset(p->open_file, 0, sizeof(p->open_file)); // TODO: 释放打开的文件
    // 释放 CWD
    if (p->cwd) {
        fs_inode_release(p->cwd); // ref--
        p->cwd = 0;
    }
}


// 为一个进程分配页表，映射跳板页与陷阱帧
// TODO: 没有处理页表映射失败的情况
pagetable_t proc_alloc_pagetable(struct proc *proc) {
    pagetable_t pagetable = vmem_create_pagetable();
    if (pagetable == 0) {
        return 0;
    }
    vmem_map_pagetable(pagetable,TRAMPOLINE, (uint64) trampoline,PTE_X | PTE_R);
    vmem_map_pagetable(pagetable,TRAPFRAME, (uint64) proc->trapframe,PTE_W | PTE_R);
    return pagetable;
}

// 一个新进程如何返回到用户态执行第一行代码
void proc_forkret(void) {
    extern char userret[];
    struct proc *p = proc_running();
    trap_user_return();
    uint64 satp = MAKE_SATP(p->pagetable);
    uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
    ((void (*)(uint64)) trampoline_userret)(satp);
}

// 找一个可用的PCB，找到就返回已经初始化好的proc指针
// 映射trampoline，分配并映射trapframe，页表，内核栈
struct proc *proc_alloc(void) {
    struct proc *p = 0;
    // 找一个未使用的PCB
    int i = 0;
    for (; i < MAX_PROCESS; i++) {
        if (procs[i].state == UNUSED) {
            p = &procs[i];
            break;
        }
    }
    // 没找到
    if (p == 0) {
        return 0;
    }
    // 找到了
    p->pid = proc_allocpid();
    p->state = USED;
    // 分配trapframe
    p->trapframe = kmem_alloc();
    if (p->trapframe == 0) {
        proc_free(p);
    }
    // 分配页表
    p->pagetable = proc_alloc_pagetable(p);
    // 分配并设置内核栈
    uint64 kstack_va = (uint64) kmem_alloc();
    if (kstack_va == 0) {
        proc_free(p);
    }
    p->kstack = KERNEL_STACK(i);
    vmem_map_pagetable(kernel_root_pagetable, p->kstack, kstack_va,PTE_W | PTE_R);
    // 设置上下文
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64) proc_forkret;
    p->context.sp = p->kstack + PAGE_SIZE;
    return p;
}

// 创建一个新的进程
// 分配pcb
// 复制用户内存
// 复制陷阱帧
// 设置子进程返回值
// 标记RUNNABLE返回
uint64 proc_fork() {
    struct proc *new_p;
    struct proc *p = proc_running();
    // 分配pcb
    new_p = proc_alloc();
    // 复制用户内存代码，数据，栈
    if (vmem_user_copy(p->pagetable, new_p->pagetable, p->size) < 0) {
        proc_free(new_p);
        return -1; // 复制失败
    } // 复制栈区
    if (vmem_stack_copy(p->pagetable, new_p->pagetable) < 0) {
        proc_free(new_p);
        return -1;
    }
    // 复制陷阱帧
    memmove(new_p->trapframe, p->trapframe, sizeof(struct trapframe));
    // 设置子进程返回值
    new_p->trapframe->a0 = 0;
    new_p->parent = p;
    new_p->size = p->size;
    new_p->state = RUNNABLE;

    // 复制打开的文件
    for (int i = 0; i < NOFILE; i++) {
        if (p->open_file[i]) {
            // 1. 复制指针：子进程指向同一个 file 结构体
            new_p->open_file[i] = file_dup(p->open_file[i]);
            // file_dup 会做 f->ref++
        }
    }

    // 复制 CWD：共享同一 inode 并增加引用计数
    if (p->cwd) {
        new_p->cwd = p->cwd;
        new_p->cwd->ref++; // ref++，防止父进程释放了子进程还在用
    }
    return new_p->pid;
}


// 创建一个内核进程，直接传入一个函数用于执行
// struct proc *kproc_create(void (*proc_func)()) {
//     struct proc *p = proc_alloc();
//     // 没找到
//     if (p == 0) {
//         return 0;
//     }
//     // 找到了
//     void *kstack_pa = kmem_alloc(); // 分配内核栈
//     void *ktrapframe_pa = kmem_alloc(); //  分配内核陷阱帧
//     if (kstack_pa == 0) {
//         // 栈分配失败
//         p->state = UNUSED;
//         return 0;
//     }
//     if (ktrapframe_pa == 0) {
//         // 陷阱帧分配失败
//         p->state = UNUSED;
//         kmem_free(kstack_pa);
//         return 0;
//     }
//     uint64 kstack_top_pa = (uint64) kstack_pa + PAGE_SIZE; // 设置栈顶
//     // 清理垃圾数据
//     memset(&p->context, 0, sizeof(p->context));
//     // 设置context返回地址和栈，陷阱帧
//     p->context.sp = kstack_top_pa;
//     p->trapframe = ktrapframe_pa;
//     p->state = RUNNABLE;
//     return p;
// }

// 问题：
// 如果应用首次被调度器选中（就是main函数初始化完毕，调用schedule），
// 假设有两个，一个TaskA，一个TaskB，调度器代码选中A，执行switch，
// A代码被唤醒执行一会，然后时钟中断发生，这个时候自动关中断了，
// A进入kernltrap，发现是时钟中断，主动yield，回到调度器，
// 调度器选择B，调用switch就直接开始执行B的第一行代码了，没有开中断


// 初始化后cpu一直在这个主循环里面寻找可运行的进程
void scheduler(void) {
    struct proc *p;
    struct cpu *c = &cpu;

    c->proc = 0;
    for (;;) {
        // 在每次循环开始时，开启中断再关闭
        // 这样，如果当前没有可运行的进程，
        // CPU 可以在 wfi 状态下等待下一次时钟中断的到来
        intr_on();
        intr_off();

        int found = 0;
        for (int i = 0; i < MAX_PROCESS; i++) {
            p = &procs[i];
            if (p->state == RUNNABLE) {
                // 找到了
                p->state = RUNNING;
                c->proc = p;
                // 换过去
                swtch(&c->context, &p->context);

                // 回来了
                c->proc = 0;
                found = 1;
            }
        }
        if (found == 0) {
            // 一整轮没找到，休眠
            asm volatile("wfi");
        }
    }
}

// 触发回到scheduler函数进行下一次调度
void sched(void) {
    if (intr_get())
        panic("sched interruptible");
    struct proc *p = cpu.proc;
    if (p->state == RUNNING)
        panic("sched: RUNNING");

    swtch(&p->context, &cpu.context); // a0 old, a1 new
}

// 进程放弃CPU
void yield(void) {
    struct proc *p = cpu.proc;
    if (p == 0) {
        return;
    }
    p->state = RUNNABLE;
    sched();
}

// 睡眠。必须在关中断的情况下被调用。
void sleep(void *channel) {
    if (intr_get())
        panic("sleep interruptible");
    struct proc *p = proc_running();

    // 1. 设置睡眠状态
    p->sleep_channel = channel;
    p->state = SLEEPING;

    sched();

    // 3. 被唤醒后，从这里继续执行
    p->sleep_channel = 0; // 清理 channel
}

// 唤醒所有睡在 channel 上的进程
// 必须在关中断的情况下被调用
void wakeup(void *channel) {
    struct proc *p;
    // 遍历进程表，找到在该频道睡眠的进程
    for (int i = 0; i < MAX_PROCESS; i++) {
        p = &procs[i];
        if (p->state == SLEEPING && p->sleep_channel == channel) {
            p->state = RUNNABLE;
        }
    }
}

// TODO: 把孩子给initproc防止成为孤儿进程
void exit(int status) {
    struct proc *p = proc_running();
    if (p == initproc) {
        panic("initproc exit");
    }
    // 关中断
    intr_off();
    p->state = ZOMBIE;
    p->exit_status = status;
    wakeup(p->parent);
    sched();
    panic("exit returned");
}

static struct proc *find_zombie_child(struct proc *parent) {
    struct proc *p;

    // 遍历整个进程表
    for (int i = 0; i < MAX_PROCESS; i++) {
        p = &procs[i];
        // 1. 检查是不是这个父进程的孩子
        if (p->parent != parent) {
            continue; // 不是，跳过
        }

        // 2. 检查这个孩子是不是僵尸
        if (p->state == ZOMBIE) {
            // 找到了！
            return p;
        }
    }
    // 3. 没找到
    return 0;
}

static int has_kids(struct proc *parent) {
    struct proc *p;

    // 遍历整个进程表
    for (int i = 0; i < MAX_PROCESS; i++) {
        p = &procs[i];
        // 只要 p->parent 是我，就说明我还有孩子
        if (p->parent == parent) {
            // 找到了，立即返回 true
            return 1;
        }
    }
    // 遍历完了，一个孩子都没有
    return 0;
}


// TODO: 应该改成push_off()，pop_off()来开关中断
int wait(uint64 status_va) {
    struct proc *p = proc_running();
    for (;;) {
        // 无限循环
        // 2. 检查孩子状态
        struct proc *zombie = find_zombie_child(p);
        if (zombie) {
            // 3. “是就返回”
            vmem_copyout(p->pagetable, status_va, (char *) &zombie->exit_status, sizeof(zombie->exit_status));
            int pid = zombie->pid;
            proc_free(zombie); // 彻底释放子进程
            return pid; // 成功返回
        }

        // 5. 检查是否还有孩子
        if (!has_kids(p)) {
            return -1; // 没有孩子，wait 失败
        }

        // 6. 孩子还在运行，睡觉
        //    (sleep 假设中断已关闭)
        sleep(p); // p->parent 是不行的，要睡在自己身上

        // 7. 被 wakeup 后，从 sleep 返回
        //    此时中断仍然是关闭的
        //    循环回到顶部 (第 2 步)，重新检查
    }
}

int proc_grow(int size) {
    struct proc *p = proc_running();
    uint64 new_size = p->size + size;
    if (size > 0) {
        // 增长堆
        if (new_size >= USER_STACK_VA) {
            return -1; // 堆栈碰撞
        }
        if (vmem_user_alloc(p->pagetable, p->size, new_size) < 0) {
            return -1; // 分配/映射失败
        }
    } else if (size < 0) {
        // 收缩堆
        vmem_user_dealloc(p->pagetable, p->size, new_size);
    }

    // 3. 更新进程大小
    p->size = new_size;
    return 0; // 成功
}
