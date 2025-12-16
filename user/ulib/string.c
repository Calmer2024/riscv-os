#include <stdarg.h>

#include "types.h"
#include "user.h"

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

char*
strcpy(char *s, const char *t)
{
    char *os;

    os = s;
    while((*s++ = *t++) != 0)
        ;
    return os;
}

int
strcmp(const char *p, const char *q)
{
    while(*p && *p == *q)
        p++, q++;
    return (uchar)*p - (uchar)*q;
}

uint
strlen(const char *s)
{
    int n;

    for(n = 0; s[n]; n++)
        ;
    return n;
}

void*
memset(void *dst, int c, uint n)
{
    char *cdst = (char *) dst;
    int i;
    for(i = 0; i < n; i++){
        cdst[i] = c;
    }
    return dst;
}

void*
memmove(void *vdst, const void *vsrc, int n)
{
    char *dst;
    const char *src;

    dst = vdst;
    src = vsrc;
    if (src > dst) {
        while(n-- > 0)
            *dst++ = *src++;
    } else {
        dst += n;
        src += n;
        while(n-- > 0)
            *--dst = *--src;
    }
    return vdst;
}

char*
strchr(const char *s, char c)
{
    for(; *s; s++)
        if(*s == c)
            return (char*)s;
    return 0;
}

char*
gets(char *buf, int max)
{
    int i, cc;
    char c;

    for(i=0; i+1 < max; ){
        cc = read(0, &c, 1);
        if(cc < 1)
            break;
        buf[i++] = c;
        if(c == '\n' || c == '\r')
            break;
    }
    buf[i] = '\0';
    return buf;
}

int
atoi(const char *s)
{
    int n;

    n = 0;
    while('0' <= *s && *s <= '9')
        n = n*10 + *s++ - '0';
    return n;
}