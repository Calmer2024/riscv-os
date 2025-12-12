#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include "types.h"
#include "riscv.h"

// 自旋锁
struct spinlock {
    uint64 locked;       // 0=unlocked, 1=locked

    // 调试用
    char *name;
    int cpu;
};

void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);

// 关/开中断 (push/pop)
void push_off(void);
void pop_off(void);

// 辅助函数
static inline void intr_off() {
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}
static inline void intr_on() {
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}
static inline int intr_get() {
    return (r_sstatus() & SSTATUS_SIE);
}

#endif