#include "../include/types.h"
#include "../include/proc.h"
#include "../include/pmm.h"
#include "../include/riscv.h"
#include "../include/memlayout.h"
#include "../include/printf.h"

struct cpu cpus[1];

// 初始化CPU核心和陷阱栈
void proc_init(void) {
    // 为核心0分配一个页面作为陷阱栈
    void* trapstack_page = alloc_page();
    if (!trapstack_page) {
        panic("proc_init: failed to allocate trap stack");
    }

    // 栈是向下生长的，所以栈顶是页面的末尾
    cpus[0].trapstack = (uint64)trapstack_page + PGSIZE;

    // 将陷阱栈的地址加载到 sscratch 寄存器
    // 当陷阱发生时，硬件不会动 sscratch，
    // 这样 kernelvec.S 就能从中找到安全栈的地址。
    w_sscratch(cpus[0].trapstack);
}