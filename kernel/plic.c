#include "../include/memlayout.h"
#include "../include/types.h"
#include "../include/printf.h"

void plic_init(void) {
    // 为 UART0、virtio0 中断设置一个优先级
    *(uint32 *) (PLIC + UART_IRQ * 4) = 1;
    *(uint32 *) (PLIC + VIRTIO0_IRQ * 4) = 1;

    // 为当前 CPU 核心（hart 0）开启 UART0、virtio0 中断
    *(uint32 *) PLIC_SENABLE(0) |= (1 << UART_IRQ);
    *(uint32 *) PLIC_SENABLE(0) |= (1 << VIRTIO0_IRQ);

    // 设置当前核心的优先级阈值为0，意味着任何优先级大于0的中断都会被接收
    *(uint32 *) PLIC_SPRIORITY(0) = 0;

    printf("plic_init: enabled UART and virtio interrupts.\n");
}

// ask the PLIC what interrupt we should serve.
int plic_claim(void) {
    int irq = *(uint32 *) PLIC_SCLAIM(0);
    return irq;
}

void plic_complete(int irq) {
    *(uint32 *) PLIC_SCLAIM(0) = irq;
}
