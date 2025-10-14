#include "../include/uart.h"
#include "../include/printf.h"
#include "../include/console.h"
#include "../include/pmm.h"
#include "../include/vm.h"
#include "../include/test.h"
#include "../include/trap.h"
#include "../include/timer.h"
#include "../include/memlayout.h"

// 来自链接脚本的外部符号
extern char _bss_start[], _bss_end[];

// 内核页表
pagetable_t kernel_pagetable;

int main(void) {
    // 启动控制台
    console_init();
    // 输出启动信息
    uart_puts("\n=== MiniOS Boot Success ===\n");
    uart_puts("Hello from C main function!\n");
    uart_puts("BSS segment cleared successfully\n");

    pmm_init();
    kvminit();     // 创建内核页表

    trap_init_hart(); // 初始化中断向量
    timer_init();     // 初始化时钟（它会自己注册中断）

    kvminithart(); // 激活页表

    // --- 切换到受保护的内核栈 ---
    printf("Switching to new kernel stack at VA: %p\n", KERNEL_STACK_TOP);
    asm volatile("mv sp, %0" : : "r"((void*)KERNEL_STACK_TOP));

    // 开中断
    w_sstatus(r_sstatus() | SSTATUS_SIE);

    run_tests();

    // 进入主循环
    while (1) {
        
        // 简单的呼吸灯效果（通过输出字符表示系统存活）
        static int counter = 0;
        if (++counter % 1000000 == 0) {
            uart_putc('.');
        }
    }
}

// 防止程序意外退出的安全措施
__attribute__((noreturn)) void system_halt(void) {
    uart_puts("\n*** System Halted ***\n");
    while (1) {
        // 无限循环，防止系统重启
        asm volatile ("wfi"); // 等待中断，为节能
    }
}
