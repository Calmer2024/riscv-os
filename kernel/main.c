#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/vm.h"
#include "../include/pmm.h"
#include "../include/test.h"

// 声明外部函数
extern void console_init(void);
extern void printf_init(void);
extern void timer_init(void);
extern void trap_init_hart(void);
extern void kvminithart(void);
extern void userinit(void);


int main(void) {
    // 1. 此时我们运行在 entry.S 设置的 stack0 上
    
    console_init();
    printf_init();
    printf("\n");
    printf("\n=== MiniOS Booting ===\n");
    printf("\n");

    pmm_init();      // 初始化物理内存分配器
    
    kvminit();       // 创建内核页表 (包含映射所有进程的内核栈)
    kvminithart();   // 开启分页 (SATP)
    
    proc_init();     // 初始化进程表锁
    trap_init_hart(); // 初始化中断向量 (stvec)
    timer_init();    // 初始化时钟中断

    printf("main: system initialized.\n");

    // 创建第一个用户进程
    userinit();

    // 3. 启动调度器
    //    注意：这一步是单行道。
    //    scheduler() 会调用 swtch
    //    swtch 会把当前的 sp (也就是 stack0) 换成 PID 2 的 kstack
    //    从此以后，stack0 就被废弃了，除非所有 CPU 都空闲。
    printf("main: starting scheduler...\n");
    
    // 开中断 (SSTATUS_SIE)，让调度器内的中断能工作
    w_sstatus(r_sstatus() | SSTATUS_SIE);

    scheduler(); 

    // 永远不该执行到这里
    panic("main: scheduler returned");
    return 0;
}