#include "uart.h"
#include "printf.h"
#include "console.h"

// 来自链接脚本的外部符号
extern char _bss_start[], _bss_end[];

static void simple_delay(long count) {
    for (volatile long i = 0; i < count; i++);
}

void test_printf_basic() {
    printf("=== printf测试 ===\n");
    printf("Testing integer: %d\n", 42);
    printf("Testing negative: %d\n", -123);
    printf("Testing zero: %d\n", 0);
    printf("Testing hex: 0x%x\n", 0xABC);
    printf("Testing string: %s\n", "Hello RISC-V");
    printf("Testing char: %c\n", 'X');
    printf("Testing percent: 100%%\n");
}

void test_printf_edge_cases() {
    printf("INT_MAX: %d\n", 2147483647);
    printf("INT_MIN: %d\n", -2147483648);
    printf("NULL string: %s\n", (char*)0);
    printf("Empty string: %s\n", "");
}

void test_console_features(void) {
    console_clear();
    printf("--- 控制台功能测试程序 ---\n");
    printf("现在开始展示 console 模块的各项功能。\n\n");
    simple_delay(50000000);

    printf("--- 1. 测试所有前景色 ---\n");
    const char* color_names[] = {"黑色", "红色", "绿色", "黄色", "蓝色", "品红", "青色", "白色"};
    const uint8_t fg_colors[] = {FG_BLACK, FG_RED, FG_GREEN, FG_YELLOW, FG_BLUE, FG_MAGENTA, FG_CYAN, FG_WHITE};

    for (int i = 0; i < 8; i++) {
        console_set_color(fg_colors[i], BG_BLACK); // 以黑色为背景进行测试
        printf("这是 [%s] 的文字。\n", color_names[i]);
    }
    console_puts(ANSI_COLOR_RESET); // 恢复默认颜色
    printf("\n");
    simple_delay(100000000);

    printf("--- 2. 测试所有背景色 ---\n");
    const uint8_t bg_colors[] = {BG_BLACK, BG_RED, BG_GREEN, BG_YELLOW, BG_BLUE, BG_MAGENTA, BG_CYAN, BG_WHITE};

    for (int i = 0; i < 8; i++) {
        console_set_color(FG_WHITE, bg_colors[i]); // 以白色为前景进行测试
        // 打印一些空格以突显背景色
        printf("  背景色为 [%s]  \n", color_names[i]);
    }
    console_puts(ANSI_COLOR_RESET); // 恢复默认颜色
    printf("\n");
    simple_delay(100000000);

    printf("将在3秒后清空屏幕...\n");
    simple_delay(150000000);
    console_clear();
    printf("屏幕已清空！\n\n");
    simple_delay(50000000);
    printf("--- 控制台功能测试结束 ---\n");
}

void test_clear(void) {
    printf("3秒后清屏...\n");
    // 简单延时
    simple_delay(150000000);
    console_clear();
    printf("屏幕已清空！\n");
}

int main(void) {
    // 启动控制台
    console_init();
    // 输出启动信息
    uart_puts("\n=== MiniOS Boot Success ===\n");
    uart_puts("Hello from C main function!\n");
    uart_puts("BSS segment cleared successfully\n");

    test_printf_basic();
    test_printf_edge_cases();
    test_console_features();
    //test_clear();

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
