#include "types.h"
#include "printf.h"
#include "syscall.h"

// 这是一个简化的系统调用处理函数
// 在未来，它会根据 a7 寄存器中的系统调用号来分发
void syscall_handler(struct trapframe *tf) {
    // a7 寄存器保存了系统调用号
    int syscall_num = tf->a7;

    printf("Syscall received! Number: %d\n", syscall_num);

    // 示例：一个简单的 write 系统调用
    if (syscall_num == 1) { // 假设 1 是 write
        printf("Simulating write syscall. Arg a0 = %d\n", tf->a0);
    }

    // 系统调用完成后，PC应该指向下一条指令
    // sepc 中保存的是 ecall 指令的地址
    tf->epc += 4;

    // 返回值通常放在 a0 寄存器中
    tf->a0 = 0; // 表示成功
}