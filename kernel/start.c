#include "../include/riscv.h"

// 定义栈空间，因为是未初始化全局变量，会被放在 .bss 段
// 让 entry.S 能找到它
__attribute__ ((aligned (16))) char stack0[4096];

// 声明 S 模式的入口函数
int main(void);

void timer_init() {
    // enable supervisor-mode timer interrupts.
    // 在 M-Mode层面，打开时钟中断的总开关。允许时钟中断事件被上报
    w_mie(r_mie() | MIE_STIE);

    // enable the sstc extension (i.e. stimecmp).
    // 开启一个高级时钟功能，允许 S-Mode 更方便地设置定时器
    w_menvcfg(r_menvcfg() | (1L << 63));

    // allow supervisor to use stimecmp and time.
    // 设置 M-Mode 的计数器使能寄存器,
    // 允许读取当前时间 (time) 和设置下一次闹钟响的时间 (stimecmp)
    w_mcounteren(r_mcounteren() | 2);

    // ask for the very first timer interrupt.
    // 设置了第一个闹钟，让它在不久的将来响起
    // r_time(): 读取一个不断增长的当前时间计数器的值。
    // w_stimecmp(...): 把“当前时间 + 间隔”这个未来的时间点，
    // 写入 stimecmp (Supervisor Timer Compare) 寄存器。
    // 硬件会自动比较 time 和 stimecmp，
    // 当 time 增长到等于 stimecmp 时，就会触发一次时钟中断。
    w_stimecmp(r_time() + 1000000);
}

// M 模式下的启动函数，由 entry.S 调用
void start(void)
{
    // 从机器模式切换到监管者内核模式
    unsigned long x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);


    // 设置 mepc (Machine Exception Program Counter) 寄存器。
    // 这个寄存器存放着从 M-Mode 返回时要跳转到的目标地址。
    w_mepc((uint64)main);

    // disable paging for now.
    // 暂时关闭虚拟内存（分页）功能
    w_satp(0);

    // delegate all interrupts and exceptions to supervisor mode.
    // 设置异常 (Exception) 委托和中断 (Interrupt) 委托寄存器。0xffff 代表“全部”。
    // 都直接交给 S-Mode 去处理
    w_medeleg(0xffff);
    w_mideleg(0xffff);

    // 设置 sie (Supervisor Interrupt Enable) 寄存器，开启 S-Mode 下的几种中断类型。
    //  S-Mode可以开始接收外部中断 (SEIE)、时钟中断 (STIE) 和软件中断 (SSIE) 了
    w_sie(r_sie() | SIE_SEIE | SIE_STIE);


    // 设置 PMP (物理内存保护) 寄存器。
    // S-Mode 可以访问所有的物理内存
    // 授权 S 模式，允许它对所有的物理内存，进行无限制的、完全的读、写、执行操作
    w_pmpaddr0(0x3fffffffffffffull);
    w_pmpcfg0(0xf);

    w_sstatus(r_sstatus() | SSTATUS_SIE);

    intr_off();

    timer_init();

    w_tp(r_mhartid());

    // switch to supervisor mode and jump to main().
    // 执行 mret (Machine Return from Exception) 指令。
    // CPU 模式瞬间切换到 S-Mode，并开始执行 main 函数的第一行代码。
    asm volatile("mret");
}

