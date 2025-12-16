//
// Created by czh on 2025/12/15.
//

#ifndef RISCV_OS_SPINLOCK_H
#define RISCV_OS_SPINLOCK_H
#include "types.h"

struct sleeplock {
    uint locked; // 0:开, 1:关
    // struct spinlock lk; // 单核，系统调用关中断，不需要自旋锁
    char *name; // 调试用
    int pid; // 拿着锁的进程的pid
};

void sleeplock_init(struct sleeplock *lk, char *name);

void sleeplock_acquire(struct sleeplock *lk);

void sleeplock_release(struct sleeplock *lk);

#endif //RISCV_OS_SPINLOCK_H
