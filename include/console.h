#ifndef CONSOLE_H
#define CONSOLE_H

// console.c
void console_getc(char c);
void console_putc(char c);
void console_flush(void);
void console_init(void);
void clear_screen(void);
void goto_xy(int x, int y);
void change_color(int color);
void clear_line(void);

#endif //CONSOLE_H
