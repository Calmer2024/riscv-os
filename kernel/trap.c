#include "../include/types.h"
#include "../include/riscv.h"
#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/trap.h"
#include "../include/syscall.h"
#include "../include/vm.h"

// 设计中断向量表结构
// 一个函数指针数组，索引是中断号
static interrupt_handler_t interrupt_handlers[MAX_IRQ];
extern void kernelvec(void);

// 异常处理函数
void handle_exception(struct trapframe *tf) {
    uint64 cause = r_scause();

    switch (cause) {
        case 2: // --- 新增：非法指令异常 (Illegal Instruction) ---
            printf("\n--- Illegal Instruction Exception ---\n");
            panic("Illegal Instruction");
            break;
        case 8: // 用户模式环境调用 (ecall from U-Mode)
            // syscall_handler(tf);
            break;

        case 12: // 指令页故障 (Instruction page fault)
        case 13: // 加载页故障 (Load page fault)
        case 15: // 存储页故障 (Store page fault)
            handle_page_fault(tf);
            break;

        default:
            printf("\n--- Unhandled Exception ---\n");
            printf("scause: 0x%lx (cause code: %d)\n", cause, cause);
            printf("sepc (faulting instruction): %p\n", tf->epc);
            printf("stval (faulting address/info): %p\n", r_stval());
            panic("kerneltrap: unhandled exception");
            break;
    }
}

// S-Mode 陷阱初始化
void trap_init_hart(void) {
    printf("trap: init hart...\n");
    // 设置S-Mode的陷阱向量入口为 kernelvec，就是把汇编代码kernelvec.S的地址写入stvec寄存器
    w_stvec((uint64)kernelvec);
    printf("trap: init hart done.\n");
}

// 中断注册机制
// irq是中断号，interrupt_handler_t是中断处理函数
void register_interrupt_handler(int irq, interrupt_handler_t handler) {
    if (irq < 0 || irq >= MAX_IRQ) {
        panic("register_interrupt_handler: invalid irq");
    }
    if (interrupt_handlers[irq] != 0) {
        panic("register_interrupt_handler: irq already registered");
    }
    interrupt_handlers[irq] = handler;
}

// 中断注销机制
void unregister_interrupt_handler(int irq) {
    // 1. 边界检查：确保中断号是有效的
    if (irq < 0 || irq >= MAX_IRQ) {
        panic("unregister_interrupt_handler: invalid irq");
    }

    // 2. 存在性检查：确保确实有一个处理函数被注册了，防止逻辑错误
    if (interrupt_handlers[irq] == 0) {
        panic("unregister_interrupt_handler: irq was not registered");
    }

    // 3. 核心操作：将中断向量表中的对应条目清零 (设置为NULL)
    interrupt_handlers[irq] = 0;
}

// 陷阱处理总入口
void kerneltrap(struct trapframe *tf) {
    uint64 scause = r_scause();
    uint64 sepc = r_sepc();

    // 判断是中断还是异常
    if (scause & (1UL << 63)) { // 中断
        uint64 interrupt_type = scause & 0x7FFFFFFFFFFFFFFF;

        // 查表，调用中断函数
        if (interrupt_type < MAX_IRQ && interrupt_handlers[interrupt_type]) {
            // 如果中断号有效且已注册处理函数，则调用它
            interrupt_handlers[interrupt_type]();
        } else {
            // 未注册的中断
            printf("kerneltrap: unhandled interrupt type %d\n", interrupt_type);
        }

    } else { // 异常
        handle_exception(tf);
    }

    w_sepc(sepc);
}


// 用户陷阱处理 (由 uservec.S 调用)
void usertrap(void) {
    uint64 scause = r_scause();
    struct proc *p = myproc();

    if(p == 0) {
        printf("usertrap: no proc\n");
        return;
    }

    // (设置 trapframe->epc，以便 sret 返回)
    p->trapframe->epc = r_sepc();

    if(scause == 8) { // User ECALL (系统调用)
        // 必须允许中断，否则 sleep 会死锁
        intr_on();

        // ecall 会使 epc + 4
        p->trapframe->epc += 4;

        syscall_dispatch(); // 调用分发器

    } else if (scause == (1L << 63 | 5)) { // S-Mode Timer Interrupt
        // 时钟中断 (来自 S-Mode)
        // (你需要配置时钟... 假设已配置)

        // 强制放弃 CPU (抢占)
        yield();

    } else if (scause == 13 || scause == 15) { // Page Fault
        handle_page_fault(p->trapframe); // 来自你的 vm.h

    } else {
        printf("usertrap: unexpected scause %p, pid=%d\n", scause, p->pid);
        // (杀死进程)
        p->state = ZOMBIE;
    }

    // 返回用户空间
    usertrapret();
}