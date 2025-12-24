#include "../include/proc.h"
#include "../include/sleeplock.h"

// 初始化睡眠锁
void sleeplock_init(struct sleeplock *lk, char *name) {
    lk->locked = 0;
    lk->name = name;
    lk->pid = 0;
}

// 获得锁
void sleeplock_acquire(struct sleeplock *lk) {
    // 内核全程关中断，所以这里是原子的
    // 不需要 acquire spinlock 来保护 locked 变量
    while (lk->locked) {
        // 如果被锁了，就在这个锁的地址上睡觉
        sleep(lk);
    }
    lk->locked = 1;
    struct proc *p = proc_running();
    lk->pid = p ? p->pid : 0;
}

void sleeplock_release(struct sleeplock *lk) {
    lk->locked = 0;
    lk->pid = 0;
    wakeup(lk); // 唤醒等待这个锁的进程
}
