#include "../include/types.h"
#include "../include/memlayout.h"
#include "../include/plic.h"
#include "../include/uart.h"
#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/riscv.h"
#include "../include/trap.h"
#include "../include/syscall.h"

volatile uint ticks;
extern char trampoline[], uservec[];

// 声明汇编入口点
extern void kernelvec();

void trap_init(void) {
    // 将 stvec 设置为我们的汇编处理函数的地址
    w_stvec((uint64) kernelvec);
    printf("trap_init: stvec set.\n");
}

// 统一处理中断的函数
void trap_interrupt_handler(uint64 scause, uint64 sepc) {
    uint8 interrupt_type = scause & 0xff;
    int irq;

    switch (interrupt_type) {
        case 5: // 时钟中断
            ticks++;
            if (ticks % 10 == 0) {
                // printf("Timer interrupt, tick %d\n", ticks);
            }
            // 预约下一次时钟中断
            wakeup((void *) &ticks);
            w_stimecmp(r_time() + 100000);
            yield();
            break;

        case 9: // 外部设备中断
            irq = plic_claim();
            // 判断中断源并调用对应的处理函数
            if (irq == UART_IRQ) {
                uart_intr(); // 调用 UART 处理函数
            } else if (irq == VIRTIO0_IRQ) {
                virtio_disk_intr(); // 调用 virtio_disk 处理函数
            } else if (irq) {
                printf("trap_kernel: unexpected interrupt irq=%d\n", irq);
            }
            if (irq) {
                plic_complete(irq);
            }
            break;

        default:
            printf("Unhandled interrupt type: %d\n", interrupt_type);
            printf("scause: %p, sepc: %p\n", (void *) scause, (void *) sepc);
            panic("trap_interrupt_handler");
            break;
    }
}

// 由汇编代码保存现场后调用的内核中断处理程序
// 不支持中断嵌套，因为中断嵌套会大大提高系统设计的复杂性
void trap_kernel() {
    // S模式原因寄存器 (Supervisor Cause)
    uint64 scause = r_scause(); // 读取原因
    // S模式异常程序计数器 (Supervisor Exception PC)
    uint64 sepc = r_sepc(); // 读取被打断的地址

    // 检查 scause 的值来判断陷阱类型
    // 判断最高位是否为1，来确定是中断还是异常
    if (scause & INTR_MASK) {
        // 中断
        trap_interrupt_handler(scause, sepc);
    } else if (scause == 13 || scause == 15) {
        // 13/15 代表 Load/Store/AMO page fault
        printf("trap_kernel: scause: %p, sepc: %p\n", (void *) scause, (void *) sepc);
        panic("Kernel tried to write/load to a invalid page.");
    } else {
        // 其他我们暂时不认识的陷阱
        printf("Unhandled kernel trap.\n");
        printf("trap_kernel: scause: %p, sepc: %p\n", (void *) scause, (void *) sepc);
        panic("trap_kernel");
    }
}

// ！！目前的trap处理，系统调用是关中断状态下运行，所以操作都是原子的，如果后续有耗时的系统调用操作需要额外处理
uint64 trap_user() {
    if ((r_sstatus() & SSTATUS_SPP) != 0) {
        panic("trap_user: not from user mode");
    }
    struct proc *p = proc_running();
    uint64 scause = r_scause(); // 读取原因
    uint64 sepc = r_sepc();

    w_stvec((uint64) kernelvec); //陷入到 kernelvec
    p->trapframe->epc = r_sepc(); // 保存trap发生时候用户程序的pc，因为epc可能会被覆盖

    if (scause & INTR_MASK) {
        // 中断
        trap_interrupt_handler(scause, sepc);
    } else {
        // 异常
        if (r_scause() == 8) {
            // 8 号异常：来自 U-Mode 的 ecall
            // ecall 指令执行完后，epc 仍然指向 ecall 本身。
            // 必须手动让它指向下一条指令。
            p->trapframe->epc += 4;
            syscall();
        } else if (scause == 13 || scause == 15) {
            // TODO: 正确处理用户异常而不是panic
            // 13/15 代表 Load/Store/AMO page fault (页面错误)
            printf("trap_user: scause: %p, sepc: %p\n", (void *) scause, (void *) sepc);
            panic("user tried to write/load to a invalid page.");
            // p->trapframe->epc += 4;
        } else {
            // 其他异常，比如访问了非法内存
            printf("trap_user: unexpected scause %p, sepc %p\n", (void *) scause, (void *) sepc);
            panic("trap_user");
        }
    }

    trap_user_return();

    uint64 satp = MAKE_SATP(p->pagetable);
    return satp; // 把satp页表存到a0寄存器中
}

void trap_user_return() {
    struct proc *p = proc_running();
    // TODO: 为什么要关中断？
    intr_off();

    // uservec地址  (uservec - trampoline) 似乎一直为0，可以不要
    uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
    w_stvec(trampoline_uservec);

    // 设置trapframe内核必须内容的值
    p->trapframe->kernel_pagetable = r_satp(); // kernel page table
    p->trapframe->kernel_sp = p->kstack + PAGE_SIZE; // process's kernel stack
    p->trapframe->kernel_trap = (uint64) trap_user;
    p->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

    // 设置返回到用户态时的状态
    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE; // enable interrupts in user mode
    w_sstatus(x);

    // 设置返回到用户态时候的pc
    w_sepc(p->trapframe->epc);
}


void test_store_page_fault(void) {
    printf_color("\n=== Running test: Write to Read-Only Memory(store page fault) ===\n", YELLOW);
    // KERNEL_BASE 是我们内核代码的起始地址，它被映射为只读+可执行。
    char *kernel_code_ptr = (char *) KERNEL_BASE + 20;
    // 2. 接下来，尝试写入。CPU会顿住，并触发一个异常，不过目前没写异常处理
    printf("Attempting to write 'X' to read-only kernel code @ %p...\n", kernel_code_ptr);
    // CPU 硬件会在这里检测到权限冲突 (W=0)，并触发一个 Store Page Fault！
    *kernel_code_ptr = 'X';
    // 如果代码能执行到这里，说明内存保护没有生效，是一个严重的 Bug！
    printf_color("!!! TEST FAILED: Write to read-only memory did NOT cause a fault! !!!\n", RED);
}
