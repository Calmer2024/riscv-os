#include "uart.h"
#include "types.h"

void uart_init(void) {
	//初始化设置两个寄存器的值
    // 配置为8N1格式（8数据位，无校验，1停止位）
    // 线路控制寄存器LCR
    *((volatile uint8_t*)(UART_BASE + 0x03)) = 0x03;
    
    // FIFO控制寄存器FCR
    // 启用并清空FIFO
    *((volatile uint8_t*)(UART_BASE + 0x02)) = 0x07;
}

// 输出字符
void uart_putc(char c) {
	// LSR是线路状态寄存器，THR是发送保持寄存器
    // 等待发送器就绪
    // 用掩码取寄存器的空标志位，若为空，则忙等待
    while (!(*((volatile uint8_t*)UART_LSR) & LSR_TX_READY)) {
        // 忙等待
    }
    
    // 写入字符到发送寄存器
    *((volatile uint8_t*)UART_THR) = c;
}

// 输出字符串
void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}
