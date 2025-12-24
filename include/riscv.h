#ifndef RISCV_H
#define RISCV_H

#include "types.h"


#define PAGE_SIZE 4096 // 页大小（字节byte）
#define LEAF_PTES (PAGE_SIZE >> 3) // 一个页表包含512个PTE
#define PAGE_SHIFT 12  // 页内偏移（12位）

#define PAGE_UP(sz)  (((sz)+PAGE_SIZE-1) & ~(PAGE_SIZE-1)) // 向上对齐
#define PAGE_DOWN(a) (((a)) & ~(PAGE_SIZE-1)) // 向下对齐

#define PTE_V (1L << 0) // 有效位
#define PTE_R (1L << 1) // 可读
#define PTE_W (1L << 2) // 可写
#define PTE_X (1L << 3) // 可运行
#define PTE_U (1L << 4) // 用户态能否使用

// 把物理地址转换为页表项中的物理地址
#define PA_TO_PTE(pa) ((((uint64)pa) >> 12) << 10)
// 把页表项中的物理地址转换为物理地址
#define PTE_TO_PA(pte) (((pte) >> 10) << 12)
// 获取PTE标志位
#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// 最大的地址，为了避免符号问题，最高位不使用
#define MAX_VIRTUAL_ADDR (1L << (9 + 9 + 9 + 12 - 1))


#ifndef __ASSEMBLER__
typedef uint64 pte_t; // 一个PTE
typedef uint64 *pagetable_t; // 指向一个4K的页，包含 512 PTEs

// 获取9bit的页索引
#define PPN_MASK 0x1FF
// 获取k级页的索引需要的位移
#define PPN_SHIFT(level) (PAGE_SHIFT + 9 * (level))
// 获取k级PNN的内容
#define PPN(va,level) ((((uint64) (va)) >> PPN_SHIFT(level)) & PPN_MASK)

// satp 寄存器相关

// 使用 riscv 的 sv39 分页方案 (模式 8)
#define SATP_SV39 (8L << 60)

// 从一个页表的物理地址创建 satp 寄存器的值
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)


// 中断
#define INTR_MASK (1L<<63)


// 写 satp 寄存器
static __attribute__((unused)) void w_satp(uint64 x) {
    asm volatile("csrw satp, %0" : : "r" (x));
}

// 刷新 TLB
static __attribute__((unused)) void sfence_vma() {
    // a0 和 a1 寄存器在这里是 zero, zero，意思是刷新所有 TLB 条目
    asm volatile("sfence.vma zero, zero");
}

static __attribute__((unused)) uint64
r_mstatus() {
    uint64 x;
    asm volatile("csrr %0, mstatus" : "=r" (x) );
    return x;
}

static __attribute__((unused)) void
w_mstatus(uint64 x) {
    asm volatile("csrw mstatus, %0" : : "r" (x));
}

static __attribute__((unused)) void
w_mepc(uint64 x) {
    asm volatile("csrw mepc, %0" : : "r" (x));
}

// Machine Exception Delegation
static __attribute__((unused)) uint64
r_medeleg() {
    uint64 x;
    asm volatile("csrr %0, medeleg" : "=r" (x) );
    return x;
}

static __attribute__((unused)) void
w_medeleg(uint64 x) {
    asm volatile("csrw medeleg, %0" : : "r" (x));
}

// Machine Interrupt Delegation
static __attribute__((unused)) uint64
r_mideleg() {
    uint64 x;
    asm volatile("csrr %0, mideleg" : "=r" (x) );
    return x;
}

static __attribute__((unused)) void
w_mideleg(uint64 x) {
    asm volatile("csrw mideleg, %0" : : "r" (x));
}

static __attribute__((unused)) uint64
r_satp() {
    uint64 x;
    asm volatile("csrr %0, satp" : "=r" (x) );
    return x;
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer

static __attribute__((unused)) uint64
r_sie() {
    uint64 x;
    asm volatile("csrr %0, sie" : "=r" (x) );
    return x;
}

static __attribute__((unused)) void
w_sie(uint64 x) {
    asm volatile("csrw sie, %0" : : "r" (x));
}

#define SIE_STIE (1L << 5)    // Supervisor Timer Interrupt Enable bit in sie
#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

static __attribute__((unused)) uint64
r_sstatus(void) {
    uint64 x;
    asm volatile("csrr %0, sstatus" : "=r" (x));
    return x;
}

static __attribute__((unused)) void
w_sstatus(uint64 x) {
    asm volatile("csrw sstatus, %0" : : "r" (x));
}

// Physical Memory Protection
static __attribute__((unused)) void
w_pmpcfg0(uint64 x) {
    asm volatile("csrw pmpcfg0, %0" : : "r" (x));
}

static __attribute__((unused)) void
w_pmpaddr0(uint64 x) {
    asm volatile("csrw pmpaddr0, %0" : : "r" (x));
}

// Supervisor Trap Cause
static __attribute__((unused)) uint64
r_scause() {
    uint64 x;
    asm volatile("csrr %0, scause" : "=r" (x) );
    return x;
}

static __attribute__((unused)) uint64
r_sepc() {
    uint64 x;
    asm volatile("csrr %0, sepc" : "=r" (x) );
    return x;
}

static __attribute__((unused)) void
w_sepc(uint64 x) {
    asm volatile("csrw sepc, %0" : : "r" (x));
}

// Supervisor Trap-Vector Base Address
// low two bits are mode.
static __attribute__((unused)) void
w_stvec(uint64 x) {
    asm volatile("csrw stvec, %0" : : "r" (x));
}


// Machine-mode Interrupt Enable
#define MIE_STIE (1L << 5)  // supervisor timer

static __attribute__((unused)) uint64
r_mie() {
    uint64 x;
    asm volatile("csrr %0, mie" : "=r" (x) );
    return x;
}

static __attribute__((unused)) void
w_mie(uint64 x) {
    asm volatile("csrw mie, %0" : : "r" (x));
}

// Machine Environment Configuration Register
static __attribute__((unused)) uint64
r_menvcfg() {
    uint64 x;
    // asm volatile("csrr %0, menvcfg" : "=r" (x) );
    asm volatile("csrr %0, 0x30a" : "=r" (x) );
    return x;
}

static __attribute__((unused)) void
w_menvcfg(uint64 x) {
    // asm volatile("csrw menvcfg, %0" : : "r" (x));
    asm volatile("csrw 0x30a, %0" : : "r" (x));
}

// Machine-mode Counter-Enable
static __attribute__((unused)) void
w_mcounteren(uint64 x) {
    asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static __attribute__((unused)) uint64
r_mcounteren() {
    uint64 x;
    asm volatile("csrr %0, mcounteren" : "=r" (x) );
    return x;
}

// machine-mode cycle counter
static __attribute__((unused)) uint64
r_time() {
    uint64 x;
    asm volatile("csrr %0, time" : "=r" (x) );
    return x;
}

// Supervisor Timer Comparison Register
static __attribute__((unused)) uint64
r_stimecmp() {
    uint64 x;
    // asm volatile("csrr %0, stimecmp" : "=r" (x) );
    asm volatile("csrr %0, 0x14d" : "=r" (x) );
    return x;
}

static __attribute__((unused)) void
w_stimecmp(uint64 x) {
    // asm volatile("csrw stimecmp, %0" : : "r" (x));
    asm volatile("csrw 0x14d, %0" : : "r" (x));
}

static __attribute__((unused)) uint64
r_mhartid() {
    uint64 x;
    asm volatile("csrr %0, mhartid" : "=r" (x) );
    return x;
}

static __attribute__((unused)) uint64
r_tp() {
    uint64 x;
    asm volatile("mv %0, tp" : "=r" (x) );
    return x;
}

static __attribute__((unused)) void
w_tp(uint64 x) {
    asm volatile("mv tp, %0" : : "r" (x));
}

// enable device interrupts
static __attribute__((unused)) void
intr_on() {
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static __attribute__((unused)) void
intr_off() {
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

static __attribute__((unused)) int
intr_get() {
    uint64 x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

#define QEMU_POWEROFF_ADDR 0x100000
#define QEMU_POWEROFF_VALUE 0x5555
#define QEMU_REBOOT_VALUE 0x7777

// 关机
static __attribute__((unused)) void shutdown(void) {
    *(volatile uint32 *) QEMU_POWEROFF_ADDR = QEMU_POWEROFF_VALUE;
}

// 重启
static __attribute__((unused)) void reboot(void) {
    *(volatile uint32 *) QEMU_POWEROFF_ADDR = QEMU_REBOOT_VALUE;
}

#endif // __ASSEMBLER__

#endif //RISCV_H
