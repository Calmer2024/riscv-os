#include "../include/console.h"
#include "../include/memlayout.h"


// copy from xv6
// the UART control registers are memory-mapped
// at address UART0. this macro returns the
// address of one of the registers.
#define Reg(reg) ((volatile unsigned char *)(UART + (reg)))

// the UART control registers.
// some have different meanings for
// read vs write.
// see http://byterunner.com/16550.html
#define RHR 0                 // receive holding register (for input bytes)
#define THR 0                 // transmit holding register (for output bytes)
#define IER 1                 // interrupt enable register
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFO control register
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
#define ISR 2                 // interrupt status register
#define LCR 3                 // line control register
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
#define LSR 5                 // line status register
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
#define LSR_TX_IDLE (1<<5)    // THR can accept another character to send

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

void uart_init(void) {
    WriteReg(IER, IER_RX_ENABLE);
}

// 简单的 uart_putc 函数 (轮询方式，不使用中断)
// 向串口输出一个字符
void uart_putc(char c) {
    // 等待 THR 寄存器变为空闲 (LSR 的第5位为1)
    while ((ReadReg(LSR) & LSR_TX_IDLE) == 0)
        ;
    // 向 THR 寄存器写入字符
    WriteReg(THR, c);
}

// 不轮询，直接写入
void uart_putc_nowait(char c) {
    WriteReg(THR, c);
}

// 打印一个字符串
void uart_puts(char *s) {
    while (*s) {
        uart_putc(*s);
        s++;
    }
}

// 打印测试
void test_uart() {
    WriteReg(THR, 'T');
    WriteReg(THR, 'E');
    WriteReg(THR, 'S');
    WriteReg(THR, 'T');
    WriteReg(THR, '\n');
}

// 从外部输入读取字符，如果没有字符则返回 -1
int uart_getc(void) {
    if ((ReadReg(LSR) & LSR_RX_READY) == 0) {
        // 接收缓冲为空
        return -1;
    }
    // 读取这个寄存器的动作，就会告诉 UART 硬件：“你的数据我收到了”
    // UART 硬件在内部就会清除“数据已准备好”的状态
    return ReadReg(RHR);
}


void uart_intr(void) {
    // 只要 UART 的接收缓冲里还有数据
    while (1) {
        int c = uart_getc(); // 尝试读取一个字符
        if (c == -1) {
            // 读不到更多字符了，说明处理完毕，跳出循环
            break;
        }

        // 在这里处理读到的字符 c
        console_getc(c);
    }
}
