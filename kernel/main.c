#include "uart.h"

// 来自链接脚本的外部符号
extern char _bss_start[], _bss_end[];

int main(void) {
    uart_init();
    
    // 输出启动信息
    uart_puts("\n=== MiniOS Boot Success ===\n");
    uart_puts("Hello from C main function!\n");
    uart_puts("BSS segment cleared successfully\n");
    
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
