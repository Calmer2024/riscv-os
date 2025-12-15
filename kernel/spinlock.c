#include "../include/spinlock.h"
#include "../include/proc.h"

void initlock(struct spinlock *lk, char *name) {
    lk->name = name;
    lk->locked = 0;
    lk->cpu = -1;
}

// 获取锁
void acquire(struct spinlock *lk) {
    // 在单核机器上，锁的实现实际上是通过关中断实现的
    push_off(); // 关中断
    // 先检查不为NULL，再检查重入，如果自己拿了自己的锁，需要处理重用
    if(lk->locked && mycpu()->proc && lk->cpu == mycpu()->proc->pid) {
        panic("acquire");
        // 实际上单核死锁最好直接死循环卡住现场，方便调试，而不是 panic (panic又会调printf导致递归)
        // while(1);
    }

    // 使用GCC内置原子操作，等待锁被释放
    // __sync_lock_test_and_set 会设置新值(1)并返回旧值
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0) {
        ; // 自旋
    }

    // 内存屏障，确保所有读写操作都在锁获取之后
    __sync_synchronize();

    // 记录谁拿了锁
    lk->cpu = (mycpu()->proc) ? mycpu()->proc->pid : -1;
}

// 释放锁
void release(struct spinlock *lk) {
    lk->cpu = -1;

    // 内存屏障，确保所有读写操作都在锁释放之前
    __sync_synchronize();

    // 原子地将 locked 设置为 0
    __sync_lock_release(&lk->locked);

    pop_off(); // 恢复中断
}


// --- 中断开关 ---
// (push_off / pop_off 允许中断处理函数也能上锁)
void push_off(void) {
    int old = intr_get();
    intr_off();
    if(mycpu()->noff == 0)
        mycpu()->intena = old;
    mycpu()->noff += 1;
}

void pop_off(void) {
    struct cpu *c = mycpu();
    c->noff -= 1;
    if(c->noff == 0 && c->intena) {
        intr_on();
    }
}