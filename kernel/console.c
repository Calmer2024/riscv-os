#include "console.h"
#include "printf.h"
#include "uart.h"

void console_init(void) {
    uart_init();
    console_clear();  // 初始化时清屏
}

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