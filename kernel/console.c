#include "../include/string.h"
#include "../include/uart.h"
#include "../include/console.h"
#include "../include/proc.h"
#include "../include/vm.h"

// 控制台缓冲区
#define CONSOLE_BUF_SIZE 128
// static保证文件作用域
static char cons_out_buf[CONSOLE_BUF_SIZE];
static int cons_out_buf_idx = 0; // 指向输出缓冲区中下一个空闲


// 刷新函数：把缓冲区内容打印到 UART，并清空缓冲区
void console_flush(void) {
    if (cons_out_buf_idx > 0) {
        // 在实际打印前，先确保字符串是 null 结尾的
        cons_out_buf[cons_out_buf_idx] = '\0';
        uart_puts(cons_out_buf);
    }
    // 重置缓冲区索引
    cons_out_buf_idx = 0;
}

void console_putc(char c) {
    if (c == '\n' || cons_out_buf_idx >= CONSOLE_BUF_SIZE - 1) {
        // 处理换行或缓冲区满，需要刷新的情况
        // 先把当前字符（换行符或最后一个字符）存入缓冲区
        if (cons_out_buf_idx < CONSOLE_BUF_SIZE - 1) {
            cons_out_buf[cons_out_buf_idx++] = c;
        }
        // 刷新缓冲区到屏幕
        console_flush();
    } else {
        // 处理普通字符
        // 存入缓冲区，但不立即刷新
        cons_out_buf[cons_out_buf_idx++] = c;
    }
}

// 我改为 读(r) 写(w) 双指针模型
static char cons_in_buf[CONSOLE_BUF_SIZE];
static uint cons_r = 0; // 读指针 (用户 syscall_read 取走的位置)
static uint cons_w = 0; // 写指针 (键盘中断 console_getc 写入的位置)
static uint cons_e = 0; // 编辑指针，用来标记“用户正在编辑但这行还没提交”的位置


// 用来接收用户的键盘输入 (中断上下文调用)
void console_getc(char c) {
    // 处理退格键 (Backspace)
    if (c == '\x7f') {
        if (cons_e != cons_w) {
            // 只能删除当前正在编辑的行，不能删除已经被读走的数据
            // 也就是只能回退 cons_e，不能回退 cons_r
            // 1. 屏幕回显删除
            // cons_out_buf_idx--; // 简单回退输出缓冲，或者直接 uart 操作
            uart_putc('\b');
            uart_putc(' ');
            uart_putc('\b');
            // 2. 写指针回退
            cons_e--;
        }
    } else {
        // 普通字符
        if (c != 0 && cons_e - cons_r < CONSOLE_BUF_SIZE) {
            c = (c == '\r') ? '\n' : c; // CR -> LF
            // 回显
            uart_putc(c);
            // 存入缓冲区
            cons_in_buf[cons_e++ % CONSOLE_BUF_SIZE] = c;
            // 只有遇到换行符或者缓冲区满了，才更新写指针 cons_w
            // 让 console_read 能看到数据
            if (c == '\n' || c == 'D' - '@' || cons_e == cons_r + CONSOLE_BUF_SIZE) {
                cons_w = cons_e;
                wakeup(&cons_r); // 唤醒等待输入的进程
            }
        }
    }
}

// 1. 实现 console_write (供 file_write 调用)
int console_write(int is_user, uint64 src, int n) {
    int i;
    struct proc *p = proc_running();
    char c;

    for (i = 0; i < n; i++) {
        // 1. 获取一个字节
        if (is_user) {
            // 从用户页表拷贝 1 字节
            if (vmem_copyin(p->pagetable, &c, src + i, 1) < 0)
                break;
        } else {
            // 内核直接访问
            c = *((char *) src + i);
        }

        // 2. 调用你现有的 putc 进行缓冲输出
        console_putc(c);
    }
    console_flush();
    return i;
}

// 2. 实现 console_read (供 file_read 调用) TODO: read还不完善
int console_read(int is_user, uint64 dst, int n) {
    uint target = n;
    struct proc *p = proc_running();
    char c;
    int c_idx = 0; // 已读取字节数

    // 循环直到读取到目标数量或者读到行结束
    while (n > 0) {
        // 等待数据提交 (cons_w 是提交点)
        while (cons_r == cons_w) {
            console_flush();
            sleep(&cons_r);
        }
        // 读出一个字符
        c = cons_in_buf[cons_r++ % CONSOLE_BUF_SIZE];
        // 遇到 EOF (Ctrl+D)
        if (c == 'D' - '@') {
            if (n < target) {
                // 如果已经读了一些数据，就把 EOF 留给下一次
                cons_r--;
            }
            break;
        }

        // 拷贝数据
        if (is_user) {
            if (vmem_copyout(p->pagetable, dst, &c, 1) < 0)
                break;
        } else {
            *(char *) dst = c;
        }
        dst++;
        n--;
        c_idx++;

        // 遇到换行符，本次读取结束 (Line Buffered)
        if (c == '\n') {
            break;
        }
    }

    return c_idx;
}

// 初始化控制台并注册
void console_init(void) {
    // 向设备表注册自己
    devsw[CONSOLE].read = console_read;
    devsw[CONSOLE].write = console_write;
}

// 实现清屏功能
void clear_screen(void) {
    // 发送“清除整个屏幕”指令，然后发送“光标回到左上角”指令
    uart_puts("\033[2J");
    uart_puts("\033[H");
}

// 光标定位 x 行 y 列
void goto_xy(int x, int y) {
    char str[24];
    sprintf(str, "\033[%d;%dH", x, y);
    uart_puts(str);
}

// 切换颜色
void change_color(int color) {
    char str[24];
    sprintf(str, "\033[%dm", color);
    uart_puts(str);
}

//清除当前行
void clear_line(void) {
    uart_puts("\033[K");
}
