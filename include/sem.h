#ifndef RISCV_OS_SEM_H
#define RISCV_OS_SEM_H

#define MAX_SEMS 64

struct semaphore {
    int used;
    int value; // 信号量的值
};

int sem_open(int init_val);

void sem_init(struct semaphore *sem, int init_val);

void sem_wait(struct semaphore *sem);

void sem_signal(struct semaphore *sem);

int sem_wait_id(int id);

int sem_signal_id(int id);

#endif //RISCV_OS_SEM_H
