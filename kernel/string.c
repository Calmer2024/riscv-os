//
// Created by czh on 25-9-19.
//
// 都是字符串工具

#include "types.h"
#include <stdarg.h>

#include "../include/printf.h"

static char digits[] = "0123456789abcdef";

// 传入buf，往buf中写入字符串，从printNumber偷来的, end 表示是否添加结束符
int sprintNumber(char *buf, long long num, int base, int sign, int end) {
    char temp[20];
    int i = 0, k = 0;
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
        temp[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        temp[i++] = '-';

    while (--i >= 0) {
        buf[k++] = temp[i];
    }
    // 添加结束符
    if (end) {
        buf[k++] = '\0';
    }
    // 返回写入的字符数
    return k;
}

void test_sprint_number(void) {
    char buf[20];
    int r = sprintNumber(buf, -123456789, 10, 1, 0);
    printf("Returned string: %s, written chars: %d\n", buf, r);
}

int sprintf(char *out, const char *fmt, ...) {
    // 定义指针
    va_list ap;
    int i;
    char *s;
    char *start = out;

    // 初始化 ap，让它指向第一个可变参数
    // 'fmt' 是最后一个固定参数
    // %d 有符号十进制整数
    // %u 无符号十进制整数
    // %o 无符号八进制整数
    // %x 无符号十六进制整数
    // %c 字符
    // %s 字符串
    // %p 指针地址
    // %% 百分号
    va_start(ap, fmt);

    // 从原本的 consputc 改成 *out
    for (i = 0; fmt[i]; i++) {
        char c = fmt[i] & 0xff;
        if (c != '%') {
            *out++ = c;
            continue;
        }
        c = fmt[++i] & 0xff;
        if (c == '\0')
            break;
        // %d
        if (c == 'd') {
            int num = va_arg(ap, int);
            int n = sprintNumber(out, num, 10, 1, 0);
            out += n;
        }
        // %u
        else if (c == 'u') {
            unsigned int num = va_arg(ap, unsigned int);
            int n = sprintNumber(out, num, 10, 0, 0);
            out += n;
        }
        // %o
        else if (c == 'o') {
            unsigned int num = va_arg(ap, unsigned int);
            int n = sprintNumber(out, num, 8, 0, 0);
            out += n;
        }
        // %x
        else if (c == 'x') {
            unsigned int num = va_arg(ap, unsigned int);
            int n = sprintNumber(out, num, 16, 0, 0);
            out += n;
        }
        // %c
        else if (c == 'c') {
            // 由于 C 语言的参数提升规则，char 作为可变参数传递时会变成 int。
            // 因此 va_arg 必须用 int 来接收
            unsigned int ch = va_arg(ap, unsigned int);
            *out++ = ch;
        }
        // %s
        else if (c == 's') {
            s = va_arg(ap, char*);
            if (s == 0) {
                s = "(null)";
            }
            while (*s) {
                *out++ = *s++; // 写入缓冲区
            }
        }
        // %%
        else if (c == '%') {
            *out++ = '%';
        }
        // 未知的占位符，原样打印
        else {
            *out++ = '%';
            *out++ = c;
        }
    }
    // 添加结束符
    *out = '\0';
    // 清理 ap
    va_end(ap);

    // 返回写入字符数
    return out - start;
}

void test_sprintf(void) {
    char str[100];
    int len = sprintf(str, "The number is %d and the string is %s.", 42, "RISC-V");
    printf("Returned string: \n%s\nWritten chars: %d", str, len);
}

void *memmove(void *dst, const void *src, uint n) {
    const char *s;
    char *d;

    if (n == 0)
        return dst;

    s = src;
    d = dst;
    if (s < d && s + n > d) {
        s += n;
        d += n;
        while (n-- > 0)
            *--d = *--s;
    } else
        while (n-- > 0)
            *d++ = *s++;

    return dst;
}

void *memset(void *dst, int c, uint n) {
    // 1. 将通用的 void* 转换为 char*
    char *cdst = dst;

    // 2. 循环 n 次，每次设置一个字节
    for (uint i = 0; i < n; i++) {
        // 将 c (通常是 int) 转换为 char 并赋值
        cdst[i] = c & 0xff;
    }

    // 3. 返回原始的目标指针
    return dst;
}

// 将 src 指向的字符串（包括'\0'）复制到 dst。
// 返回值：dst
char *strcpy(char *dst, const char *src) {
    char *ret = dst; // 保存原始的 dst 指针用于返回
    while ((*dst++ = *src++));
    return ret;
}

char*
strncpy(char *s, const char *t, int n)
{
    char *os;

    os = s;
    while(n-- > 0 && (*s++ = *t++) != 0)
        ;
    while(n-- > 0)
        *s++ = 0;
    return os;
}

// 比较两个字符串 s1 和 s2。
// 返回值：
//   < 0  如果 s1 < s2
//   = 0  如果 s1 == s2
//   > 0  如果 s1 > s2
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    // 在循环结束的地方，比较第一个不相同的字符
    // 转换为 unsigned char 是为了防止带符号的 char 扩展为负数导致非预期的结果
    return *(const unsigned char *) s1 - *(const unsigned char *) s2;
}

int
strlen(const char *s)
{
    int n;

    for(n = 0; s[n]; n++)
        ;
    return n;
}
