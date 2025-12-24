#ifndef PRINTF_H
#define PRINTF_H

// 控制台颜色定义
#define BLACK 30
#define RED 31
#define GREEN 32
#define YELLOW 33
#define BLUE 34
#define PURPLE 35

// printf.c
int printf_color(const char *fmt, int color, ...);
int printf(const char *fmt, ...);
void panic(char *s);
void test_printf(void);
void test_printf_timer(void);

#endif //PRINTF_H
