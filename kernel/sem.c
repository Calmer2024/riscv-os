#include "../include/proc.h"
#include "../include/sem.h"

struct semaphore sems[MAX_SEMS];

void sem_init(struct semaphore *sem, int init_val) {
    sem->value = init_val;
}

// 目前不需要锁，因为目前所有系统调用都是在关中断情况下进行的
void sem_wait(struct semaphore *sem) {
    // 必须使用 while 循环！
    // (防止“虚假唤醒”或多个进程同时被唤醒)
    while (sem->value == 0) {
        // 资源为 0，我们必须睡觉
        // 我们睡在 "sem" 这个地址上
        // sleep() 会原子地 (睡觉 + 重新调度)
        sleep(sem);
        // 当被 wakeup(sem) 唤醒后，
        // sleep() 返回，此时中断仍然是关闭的。
        // 循环回到 while 顶部，重新检查 sem->value
    }
    // 成功获取资源
    sem->value--;
}

void sem_signal(struct semaphore *sem) {
    sem->value++;
    // 唤醒所有睡在 "sem" 上的进程
    wakeup(sem);
}

// 找到一个可用的信号量，初始化并返回句柄
int sem_open(int init_val) {
    for (int i = 0; i < MAX_SEMS; i++) {
        // 你要找 used == 0 的，而不是 != 0
        if (sems[i].used == 0) {
            sems[i].used = 1;
            sem_init(&sems[i], init_val);
            return i; // 返回 ID
        }
    }
    return -1; // 没有可用的信号量
}

// 通过 ID 调用 sem_wait
int sem_wait_id(int id) {
    if (id < 0 || id >= MAX_SEMS || sems[id].used == 0) {
        return -1; // 非法 ID
    }
    // 调用原语
    sem_wait(&sems[id]);
    return 0; // 成功
}

// 通过 ID 调用 sem_signal
int sem_signal_id(int id) {
    if (id < 0 || id >= MAX_SEMS || sems[id].used == 0) {
        return -1; // 非法 ID
    }
    // 调用原语
    sem_signal(&sems[id]);
    return 0; // 成功
}