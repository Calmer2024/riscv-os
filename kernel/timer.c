#include "../include/types.h"
#include "../include/printf.h"
#include "../include/riscv.h"
#include "../include/trap.h"

// 使用内联汇编执行SBI调用
// OpenSBI检查 a7 和 a6 寄存器，明白S-Mode是想调用“时间扩展”的“设置定时器”功能。OpenSBI读取 a0 寄存器，得到我们设定的目标时间。核心操作：OpenSBI执行只有M-Mode才能执行的特权指令，将这个目标时间写入到物理时钟硬件 (CLINT) 的 mtimecmp 寄存器中。
static inline uint64 sbi_call(uint64 extension, uint64 function, uint64 arg0) {
    register uint64 a0 asm("a0") = arg0;    // 参数 arg0 放入寄存器 a0
    register uint64 a6 asm("a6") = function;    // 功能号 function 放入 a6
    register uint64 a7 asm("a7") = extension;   // 扩展 ID extension 放入 a7
    asm volatile(
        "ecall"     // 触发 SBI 调用
        : "+r"(a0)  // a0 同时作为输入和输出（返回值）
        : "r"(a6), "r"(a7) // 输入参数 a6 和 a7
        : "memory" // 告诉编译器内存可能被修改
    );
    return a0;  // 返回 SBI 调用的结果
}

#define SBI_EXT_TIME 0x54494D45
#define SBI_SET_TIMER 0

// 固件接口，SBI 调用接口
static void sbi_set_timer(uint64 time) {
    sbi_call(SBI_EXT_TIME, SBI_SET_TIMER, time);
}

// get_time 函数可以直接通过 rdtime 指令实现，效率更高
static uint64 get_time(void) {
    uint64 time;
    asm volatile("rdtime %0" : "=r"(time));
    return time;
}

#define TIMER_INTERVAL 1000000 // 约 1/10 秒 (取决于时钟频率)

static uint64 ticks = 0;
volatile uint64 g_ticks_count = 0;

// 时钟中断处理函数
void clockintr(void) {
    // 1. 立即设置下一次时钟中断，确保中断不会丢失
    sbi_set_timer(get_time() + TIMER_INTERVAL);
    
    // 2. 更新系统时间（时钟滴答数）
    ticks++;
    g_ticks_count++;
    
    // 3. 触发任务调度 (未来)
    // yield();

    // 打印信息用于调试
    if (ticks % 100 == 0) {
        printf("tick %u\n", ticks);
    }
}

// 时钟模块初始化
void timer_init(void) {
    printf("timer: init...\n");
    // 1. 注册时钟中断处理函数到新的中断框架
    register_interrupt_handler(IRQ_S_TIMER, clockintr);

    // 2. 设置第一个时钟中断
    sbi_set_timer(get_time() + TIMER_INTERVAL);

    // 3. 在 sie 寄存器中使能 S-Mode 的时钟中断
    w_sie(r_sie() | SIE_STIE);
    printf("timer: init done.\n");
}