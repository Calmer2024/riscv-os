#include "../include/console.h"
#include "../include/fs.h"
#include "../include/printf.h"
#include "../include/uart.h"
#include "../include/plic.h"
#include "../include/trap.h"
#include "../include/kalloc.h"
#include "../include/vm.h"
#include "../include/proc.h"
#include "../include/test.h"



int main(void) {
    printf("\n=== MiniOS Booting ===\n");
    uart_init();
    plic_init();
    console_init();
    trap_init(); // 初始化trap，内核中断跳转到kernelvec.S
    kmem_init(); // 物理内存管理初始化
    vmem_init(); // 虚拟内存页表初始化
    vmem_enable_paging(); // 启用分页
    virtio_disk_init();
    fs_init(ROOTDEV, 0);
    file_init();

    printf("main: system initialized.\n");

    // 创建第一个用户进程
    proc_userinit();

    // 3. 启动调度器
    //    注意：这一步是单行道。
    //    scheduler() 会调用 swtch
    //    swtch 会把当前的 sp (也就是 stack0) 换成 PID 2 的 kstack
    //    从此以后，stack0 就被废弃了，除非所有 CPU 都空闲。
    printf("main: starting scheduler...\n");

    scheduler(); 

    // 永远不该执行到这里
    panic("main: scheduler returned");
    return 0;
}