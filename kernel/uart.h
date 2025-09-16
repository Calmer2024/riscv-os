#ifndef UART_H
#define UART_H

// UART寄存器基地址
#define UART_BASE 0x10000000

#define UART_THR (UART_BASE + 0x00) // 表示发送保持寄存器
#define UART_LSR (UART_BASE + 0x05) // 表示线路状态寄存器

#define LSR_TX_READY 0x20 // 发送保持寄存器空标志

void uart_init(void); 
void uart_putc(char c); // 字符输出函数
void uart_puts(const char *s); //字符串输出函数

#endif
