#include "../include/stdarg.h"
#include <stdint.h>
#include "../include/string.h"
#include "../include/printf.h"
#include "../include/console.h"


// 这是一个全局静态数组，用作数字到字符的查找表。例如，数字 10 在十六进制（base 16）中对应字符 a，就可以通过 digits[10] 得到
static char digits[] = "0123456789abcdef";

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

// 辅助函数：将一个64位整数 num 按照指定的 base（进制，如10或16）转换成字符串，并通过 console_putc 逐字符打印。sign表示是否处理符号
static void print_number(int64_t num, int base, int sign) {
    char buf[20];// 定义一个大小为20的字符数组作为临时缓冲区；一个64位的有符号整数，其最大值 LLONG_MAX 大约是 9 x 10^18，总共19位。加上可能有的负号，20个字符足够容纳任何64位整数的10进制表示。
    int i = 0;

    if (sign && num < 0) {
        console_putc('-');
        // // 处理INT_MIN特例，INT_MIN的绝对值比INT_MAX绝对值大1
        if (num == INT64_MIN) {
            // 特殊处理INT64_MIN避免溢出
            num = INT64_MAX;
            buf[i++] = digits[(num % base) + 1]; // 修正最后一位
            num /= base;
        } else {
            num = -num;
        }
    }

    // 数字转换
    do {
        buf[i++] = digits[num % base];
    } while ((num /= base) > 0);

    // 逆序输出
    while (--i >= 0)
        console_putc(buf[i]);
}

// 辅助函数：将一个64位整数 num 转换为指定 base（进制）的字符串表示，然后将这个字符串写入到 buf 指针指向的内存地址。最后，它返回写入的字符总数。
static int print_number_to_buf(char *buf, int64_t num, int base, int sign) {
    char tmp[20];
    int i = 0;

    // 处理负数
    if (sign && num < 0) {
        *buf++ = '-';
        num = -num;
    }

    // 转换数字
    do {
        tmp[i++] = digits[num % base];
    } while ((num /= base) > 0);

    // 逆序拷贝
    int start = i;
    while (--i >= 0)
        *buf++ = tmp[i];

    return start;
}

// 辅助函数：逐个字符地将一个字符串打印到控制台，并且能安全地处理 NULL 指针
static void print_string(const char *s) {
    if (!s) s = "(null)";
    while (*s) console_putc(*s++);
}
// 用于格式化输出字符串到控制台。它能解析一个包含普通字符和特殊格式说明符（如 %d, %s）的字符串，并从后续的可变参数中提取相应的值来替换这些说明符。
int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);// 初始化 ap。这个宏告诉 ap 从哪里开始，也就是从最后一个已命名的参数 fmt 之后开始。现在，ap 就准备好读取第一个可变参数了。

    while (*fmt) {
        if (*fmt != '%') {
            console_putc(*fmt++);
            continue;
        }

        fmt++; // 跳过%
        switch (*fmt++) {
            case 'd': print_number(va_arg(ap, int), 10, 1); break;
            case 'u': print_number(va_arg(ap, int), 10, 0); break;
            case 'x': print_number(va_arg(ap, int), 16, 0); break;
            case 's': print_string(va_arg(ap, char*)); break;
            case 'c': console_putc(va_arg(ap, int)); break;
            case 'p': print_pointer(va_arg(ap,unsigned long )); break;
            case '%': console_putc('%'); break;
            default:  // 未知格式
                console_putc('%');
                console_putc(fmt[-1]);
        }
    }

    // 清理操作
    va_end(ap);
    return 0;
}

// 这个函数是 printf 的一个变体，名为 sprintf (string printf)，它的功能是将格式化的内容输出到一个字符串缓冲区中，而不是直接打印到控制台。
int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    char *start = buf;

    va_start(ap, fmt);
    while (*fmt) {
        if (*fmt != '%') {
            *buf++ = *fmt++;
            continue;
        }

        fmt++; // 跳过%
        switch (*fmt++) {
            case 'd':// 有符号十进制
                buf += print_number_to_buf(buf, va_arg(ap, int), 10, 1);
                break;
            case 's': {// 字符串
                char *s = va_arg(ap, char*);
                if (!s) s = "(null)";
                strcpy(buf, s);
                buf += strlen(s);
                break;
            }
            case 'u': // 无符号十进制
                buf += print_number_to_buf(buf, va_arg(ap, unsigned int), 10, 0);
                break;
            case 'x': // 十六进制
                buf += print_number_to_buf(buf, va_arg(ap, unsigned int), 16, 0);
                break;
            case 'c': // 字符
                // char 在可变参数中被提升为 int
                *buf++ = (char)va_arg(ap, int);
                break;
            case '%': // 百分号
                *buf++ = '%';
                break;
            default: // 处理未知的格式说明符
                *buf++ = '%';
                *buf++ = fmt[-1]; // fmt[-1] 获取刚刚被 *fmt++ 消耗掉的那个未知字符
                break;
        }
    }
    *buf = '\0';
    va_end(ap);
    return buf - start;
}

void panic(const char *s) {
    printf("PANIC: %s\n", s);
    while (1); // 挂起系统
}
