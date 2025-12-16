#include "../include/printf.h"
#include "../include/timer.h"
#include "../include/console.h"
#include <stdarg.h>

#include "../include/riscv.h"


static char digits[] = "0123456789abcdef";

// 参考：kernel/printf.c, printint()
// 打印一个int整形数
// num：要打印的数值（带符号）
// base：进制数，10-十进制，16-十六进制
// sign：是否为有符号数标志
void print_number(long long num, int base, int sign) {
    char buf[20];
    int i = 0;
    unsigned long long x;
    // 处理符号
    if (sign && num < 0) {
        sign = 1;
        x = -num;
    } else {
        sign = 0;
        x = num;
    }
    do {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i++] = '-';

    while (--i >= 0)
        console_putc(buf[i]);
}

// 每次取出 4 个 bit，转换成一个 0-15 的数字，然后查表得到对应的十六进制字符
void print_pointer(unsigned long num) {
    int i;
    // 0x前缀
    console_putc('0');
    console_putc('x');

    for (i = 0; i < (sizeof(unsigned long) * 2); i++, num <<= 4) {
        int x = (int) ((num >> (sizeof(unsigned long) * 8 - 4)) & 0x0f); // 取x最高位
        char ch = digits[x]; // 查表转换为字符
        console_putc(ch);
    }
}

// 字符串输出是否可以批量发送？
// 我们现在的 printf 每生成一个字符，就调用一次 consputc -> uart_putc，
// 这个过程涉及到多次函数调用开销。
// 一个更高效的方式是先把整个格式化好的字符串在一个内存缓冲区（char buf[128]）里拼接完成，
// 然后一次性调用一个 console_puts(buf) 函数。
// 这个 console_puts 还可以进一步优化

// 数字转换是否可以查表优化？
// 数字查表带来的优化不大，最大的开销是反复除法取余

// 格式解析是否可以预编译？
// 在编译时或第一次运行时，就把 "hello %d" 这样的格式字符串，解析成一种中间格式，
// 后续再调用时就直接使用这个解析好的结果，避免了重复的 % 查找。
// 对于我们的内核来说感觉有些舍本逐末了。

// 为什么需要分层？每层的职责如何划分？
// 为了解耦和复用。分层使得每一层都只关心自己的任务，而不需要知道其他层的实现细节。
// 职责划分：格式化层(printf) 负责处理数据类型和格式；
// 控制台层(consputc) 负责处理控制台逻辑（如退格、ANSI码）；
// 硬件层(uartputc) 负责与物理硬件通信。

// 如果要支持多个输出设备（串口+显示器），架构如何调整？
// 在控制台层 (consputc) 进行分发。
// consputc 函数将不再是直接调用 uart_putc，
// 而是会同时调用 uart_putc 和一个新的 vga_putc (显示器驱动) 函数，
// 把同一个字符广播给所有注册的输出设备。
// 这样，printf 依然只需要调用 consputc，完全不知道底层有两个设备在同时工作。

// 数字转字符串为什么不用递归？
// 为了防止内核栈溢出。递归会消耗与数字位数成正比的栈空间，
// 在资源有限且极其注重稳定性的内核中，这是一个巨大的风险。
// 循环的方式使用固定大小的栈空间，更加安全可控。

// 如何在不使用除法的情况下实现进制转换？
// 对于2的幂的进制（如二进制%b、十六进制%x），
// 可以直接用位运算（& 和 >>）来代替除法和取余，效率极高，比如printPointer
// 对于十进制就比较复杂了。存在一些纯位运算的算法（如“Double Dabble”），
// 但它们通常比现代 CPU 上的单条除法指令要慢且复杂。
// 所以，对于十进制，直接用 / 和 % 是最明智的选择。

// 当前实现的性能瓶颈在哪里？
// 瓶颈在 uartputc 内部的“忙等待”循环
// while((ReadReg(LSR) & LSR_TX_IDLE) == 0) 。
// CPU 在这里空转，等待串口硬件“有空”，完全浪费了 CPU 时间。

// 如何设计一个高效的缓冲机制？
// 使用中断和环形缓冲区。
// 创建一个内存环形缓冲区。
// printf 把字符快速写入这个缓冲区就直接返回，不等待硬件。
// 当 UART 发送完一个字符，硬件会产生一个“发送完成”中断。
// 我们编写一个中断服务程序 (ISR)，它会在中断时被触发，
// 从环形缓冲区里取出下一个字符，交给 UART 去发送。
// 这样，CPU 就不再需要等待慢速的 UART 了，
// 打印操作变成了异步的，大大提升了系统性能。
// 但是测试发现串口发送速度似乎很快，并没有浪费多少CPU时间

// printf 遇到 NULL 指针应该如何处理？
// printf 内部检查指针是否为 NULL (0)。
// 如果是，就打印一个固定的字符串，"(null)"。

// 格式字符串错误时的恢复策略是什么？
// 当遇到不认识的格式符，比如 "%q"，printf 应该将 "%q" 原样输出，
// 然后继续解析后面的字符串，这样既不会崩溃，
// 也向上层暴露了错误，方便其定位和修复。

// 把printf核心功能提取出来复用代码
// %d 有符号十进制整数
// %u 无符号十进制整数
// %o 无符号八进制整数
// %x 无符号十六进制整数
// %c 字符
// %s 字符串
// %p 指针地址 %ld lld ......
// %% 百分号

// 当前支持32位数
int vprintf(const char *fmt, va_list ap) {
    int i;
    char *s;

    // 为什么 xv6 要 fmt[i] & 0xff ?
    // & 0xff 是一个保险措施，它屏蔽了不同平台和编译器对 char 类型的处理差异，
    // 确保无论发生什么类型的提升，我们得到的永远是一个纯粹的、无符号的 8 位字节值
    for (i = 0; fmt[i]; i++) {
        char c = fmt[i] & 0xff;
        if (c != '%') {
            console_putc(c);
            continue;
        }

        c = fmt[++i] & 0xff;
        if (c == '\0')
            break;

        if (c == 'd') {
            int num = va_arg(ap, int);
            print_number(num, 10, 1);
        } else if (c == 'u') {
            unsigned int num = va_arg(ap, unsigned int);
            print_number(num, 10, 0);
        } else if (c == 'o') {
            unsigned int num = va_arg(ap, unsigned int);
            print_number(num, 8, 0);
        } else if (c == 'x') {
            unsigned int num = va_arg(ap, unsigned int);
            print_number(num, 16, 0);
        } else if (c == 'p') {
            unsigned long ptr = va_arg(ap, unsigned long);
            print_pointer(ptr);
        }
        // 由于 C 语言的参数提升规则，char 作为可变参数传递时会变成 int。
        // 因此 va_arg 必须用 int 来接收
        else if (c == 'c') {
            unsigned int ch = va_arg(ap, unsigned int);
            console_putc(ch);
        } else if (c == 's') {
            s = va_arg(ap, char*);
            if (s == 0) {
                s = "(null)";
            }
            for (; *s; s++)
                console_putc(*s);
        } else if (c == '%') {
            console_putc('%');
        } else {
            console_putc('%');
            console_putc(c);
        }
    }
    return 0;
}

// 使用-fno-builtin编译选项，禁止编译器替换函数调用成标准库函数的puts导致链接错误
int printf(const char *fmt, ...) {
    // 定义指针
    // 初始化 ap，让它指向第一个可变参数
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    // 有的没换行符，刷新控制台缓冲区
    console_flush();
    return 0;
}

// 前景色：30-37（黑、红、绿、黄、蓝、紫、青、白）
// 背景色：40-47（黑、红、绿、黄、蓝、紫、青、白）
// 效果： 0：重置所有属性 1：高亮 4：下划线 5：闪烁 7：反显
int printf_color(const char *fmt, int color, ...) {
    // 先切换颜色
    change_color(color);
    // 使用可变参数调用 vprintf
    va_list ap;
    va_start(ap, color);
    vprintf(fmt, ap);
    va_end(ap);
    // 换回去
    change_color(0);
    return 0;
}



void panic(char *s) {
    printf_color("panic: %s\n", RED, s);
    printf("\n");
    intr_off(); // 最好还是先关中断

    // 调用 SBI 关机
    shutdown();

    while(1) {
        asm volatile("wfi");
    }
}

void test_printf(void) {
    printf("Hello, this is a test for printf.\n");
    printf("中文能正常输出吗？\n");
    printf("The number is %d and the string is %s.\n", 123, "ABC");
    printf("数字是 %d ，字符串是 %s.\n", 123, "ABC");
    printf("Another number: %d\n", -456);
    printf("INT_MIN test: %d\n", -2147483648);
    printf("Printing a percent sign: %%\n");
    printf("Unsigned int: %u\n", -1);
    printf("Oct Test: %o\n", 255);
    printf_color("Hex Test: %x\n", 34, 0xabcdef);
    printf("Char Test: %c\n", 'c');
    printf("Null string test: %s, then a uint %u\n", (char *) 0, -1);
    printf("Unknown type test: %y%d\n", 666);
    printf("Empty string test: %s\n", "");
    printf("Pointer test01: %p\n", (void *) 0xdeadbeef);
    int a = 123;
    printf("Pointer test02: %p\n", &a);
}

void test_printf_timer(void) {
    unsigned long long start, end, min_diff = 0xFFFFFFFFFFFFFFFF; // 设为最大值
    int i;

    // --- 预热阶段 ---
    for (i = 0; i < 10; i++) {
        test_printf();
    }

    // --- 正式测试 ---
    for (i = 0; i < 100; i++) {
        start = get_time();
        test_printf();
        end = get_time();

        if ((end - start) < min_diff) {
            min_diff = end - start; // 只保留最小的耗时
        }
    }

    printf("\n--- 最好性能测试结果 ---\n");
    printf("最小占用周期: %d\n", (int) min_diff);

    // 在一个没有任何其他操作的、最紧凑的循环里，连续发送大量字符
    // 事实证明没用丢失任何A，可能是QEMU进行了优化
    // for (int ii = 0; ii < 200; ii++) {
    //     uart_putc_nowait('A'); // 使用不等待的版本
    // }
}
