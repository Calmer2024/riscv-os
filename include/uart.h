#ifndef UART_H
#define UART_H

// uart.c
void uart_init(void);
void uart_puts(char *s);
void uart_putc(char c);
void uart_putc_nowait(char c);
void uart_intr(void);

#endif //UART_H
