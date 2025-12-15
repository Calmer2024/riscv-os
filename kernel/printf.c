#include "../include/stdarg.h"
#include <stdint.h>
#include "../include/string.h"
#include "../include/printf.h"
#include "../include/console.h"
#include "../include/spinlock.h" // 需要自旋锁

// 定义一个用于 printf 的锁
struct spinlock pr_lock;

// 标记锁是否已经初始化，避免在系统启动极早期（锁还没init）时使用锁导致崩溃
int locking_allowed = 0;

// 标记是否正在 Panic，如果在 Panic 状态下，不再获取锁，防止死锁
volatile int panicked = 0;

// 初始化 printf 锁，需要在 main.c 的 console_init() 后调用
void printf_init(void) {
    initlock(&pr_lock, "pr");
    locking_allowed = 1;
}

// 这是一个全局静态数组，用作数字到字符的查找表。
static char digits[] = "0123456789abcdef";

void print_pointer(unsigned long num) {
    int i;
    console_putc('0');
    console_putc('x');
    for (i = 0; i < (sizeof(unsigned long) * 2); i++, num <<= 4) {
        int x = (int) ((num >> (sizeof(unsigned long) * 8 - 4)) & 0x0f);
        char ch = digits[x];
        console_putc(ch);
    }
}

static void print_number(int64_t num, int base, int sign) {
    char buf[20];
    int i = 0;
    if (sign && num < 0) {
        console_putc('-');
        if (num == INT64_MIN) {
            num = INT64_MAX;
            buf[i++] = digits[(num % base) + 1];
            num /= base;
        } else {
            num = -num;
        }
    }
    do {
        buf[i++] = digits[num % base];
    } while ((num /= base) > 0);
    while (--i >= 0)
        console_putc(buf[i]);
}

static int print_number_to_buf(char *buf, int64_t num, int base, int sign) {
    char tmp[20];
    int i = 0;
    if (sign && num < 0) {
        *buf++ = '-';
        num = -num;
    }
    do {
        tmp[i++] = digits[num % base];
    } while ((num /= base) > 0);
    int start = i;
    while (--i >= 0)
        *buf++ = tmp[i];
    return start;
}

static void print_string(const char *s) {
    if (!s) s = "(null)";
    while (*s) console_putc(*s++);
}

// printf
// 现在 printf 是线程安全的
int printf(const char *fmt, ...) {
    va_list ap;
    int locking = locking_allowed;

    // 如果已经发生了 panic，就不要再加锁了，防止死锁
    // (例如：printf 拿了锁 -> 中断 -> panic -> panic调printf -> 等锁 -> 死锁)
    if(panicked) {
        locking = 0;
    }

    // 【1. 获取锁】
    // acquire 通常会自动关闭中断 (push_off)，防止持锁期间被中断打断
    if(locking) {
        acquire(&pr_lock);
    }

    va_start(ap, fmt);

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
                if(fmt[-1]) console_putc(fmt[-1]);
        }
    }

    va_end(ap);

    // 【2. 释放锁】
    if(locking) {
        release(&pr_lock);
    }

    return 0;
}

// sprintf 操作的是局部 buffer，不需要加锁（除非多线程操作同一个buffer，那是调用者的问题）
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
            case 'd':
                buf += print_number_to_buf(buf, va_arg(ap, int), 10, 1);
                break;
            case 's': {
                char *s = va_arg(ap, char*);
                if (!s) s = "(null)";
                strcpy(buf, s);
                buf += strlen(s);
                break;
            }
            case 'u':
                buf += print_number_to_buf(buf, va_arg(ap, unsigned int), 10, 0);
                break;
            case 'x':
                buf += print_number_to_buf(buf, va_arg(ap, unsigned int), 16, 0);
                break;
            case 'c':
                *buf++ = (char)va_arg(ap, int);
                break;
            case '%':
                *buf++ = '%';
                break;
            default:
                *buf++ = '%';
                *buf++ = fmt[-1];
                break;
        }
    }
    *buf = '\0';
    va_end(ap);
    return buf - start;
}

void panic(const char *s) {
    // 设置 panic 标志
    panicked = 1;

    // 关闭中断，确保不会切走
    asm volatile("csrw sie, zero");

    // 进入 console panic 模式
    console_enter_panic();

    printf("\n[PANIC] %s\n", s);

    // 可选：强制刷新（如果你将来恢复 ring buffer）
    // console_flush();

    while (1)
        ;
}
