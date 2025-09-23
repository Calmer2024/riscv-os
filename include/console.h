#ifndef CONSOLE_H
#define CONSOLE_H

#include "types.h"

// ANSI颜色代码定义
#define ANSI_COLOR_RESET   "\033[0m"
#define ANSI_CLEAR_SCREEN  "\033[2J\033[H"

// 前景色定义
#define FG_BLACK   30
#define FG_RED     31
#define FG_GREEN   32
#define FG_YELLOW  33
#define FG_BLUE    34
#define FG_MAGENTA 35
#define FG_CYAN    36
#define FG_WHITE   37

// 背景色定义
#define BG_BLACK   40
#define BG_RED     41
#define BG_GREEN   42
#define BG_YELLOW  43
#define BG_BLUE    44
#define BG_MAGENTA 45
#define BG_CYAN    46
#define BG_WHITE   47

void console_init(void);
void console_putc(char c);
void console_puts(const char *s);
void console_clear(void);
void console_set_color(uint8_t fg_color, uint8_t bg_color);

#endif