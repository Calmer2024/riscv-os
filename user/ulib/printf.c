//
// Created by czh on 2025/10/22.
//

#include <stdarg.h>

#include "user.h"


static char digits[] = "0123456789abcdef";

static void
putc(char c)
{
    write(1, &c, 1);
}

// 与内核的printf差不多，就是改成了用系统调用
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
        putc(buf[i]);
}

// 每次取出 4 个 bit，转换成一个 0-15 的数字，然后查表得到对应的十六进制字符
void print_pointer(unsigned long num) {
    int i;
    // 0x前缀
    putc('0');
    putc('x');

    for (i = 0; i < (sizeof(unsigned long) * 2); i++, num <<= 4) {
        int x = (int) ((num >> (sizeof(unsigned long) * 8 - 4)) & 0x0f); // 取x最高位
        char ch = digits[x]; // 查表转换为字符
        putc(ch);
    }
}

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
            putc(c);
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
            putc(ch);
        } else if (c == 's') {
            s = va_arg(ap, char*);
            if (s == 0) {
                s = "(null)";
            }
            for (; *s; s++)
                putc(*s);
        } else if (c == '%') {
            putc('%');
        } else {
            putc('%');
            putc(c);
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
    return 0;
}
