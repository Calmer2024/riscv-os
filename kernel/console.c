#include "../include/console.h"
#include "../include/printf.h"
#include "../include/uart.h"

console_buffer_t console_out_buf;

void console_init(void) {
    uart_init();
    console_clear();  // 初始化时清屏
    console_out_buf.head = console_out_buf.tail = 0;
}

void console_flush(void) {
    while (console_out_buf.tail != console_out_buf.head) {
        uart_putc(console_out_buf.buf[console_out_buf.tail]);
        console_out_buf.tail = (console_out_buf.tail + 1) % CONSOLE_BUF_SIZE;
    }
}

// void console_putc(const char c) {
//     // 检查缓冲区是否已满
//     if ((console_out_buf.head + 1) % CONSOLE_BUF_SIZE == console_out_buf.tail) {
//         // 缓冲区满时的处理策略：
//         // 1. 直接输出到硬件（确保不丢失数据）
//         uart_putc(c);
//     } else {
//         // 正常缓冲写入
//         console_out_buf.buf[console_out_buf.head] = c;
//         console_out_buf.head = (console_out_buf.head + 1) % CONSOLE_BUF_SIZE;
//
//         // 自动刷新条件（可选）
//         if (console_out_buf.head == console_out_buf.tail) {
//             console_flush();
//         }
//     }
// }

void console_putc(char c) {
    uart_putc(c);
}

void console_puts(const char *s) {
    while (*s) {
        console_putc(*s++);
    }
}

void console_clear(void) {
    console_puts("\033[2J\033[H");
}

void console_set_color(uint8_t fg_color, uint8_t bg_color) {
    char buf[16];
    sprintf(buf, "\033[%d;%dm", fg_color, bg_color);
    console_puts(buf);
}