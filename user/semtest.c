//
// Created by czh on 2025/10/28.
//

#include "ulib/user.h"

#define BUFFER_SIZE 5
#define NUM_ITEMS 10
#define SHM_FILE "shm_buf"

// 定义存储在文件里的结构
struct shared_state {
    int in;
    int out;
    int buffer[BUFFER_SIZE];
};

// 信号量 ID
int mutex; 
int empty; 
int full;  

// 辅助函数：读取共享状态
void read_state(struct shared_state *st) {
    int fd = open(SHM_FILE, O_RDONLY);
    if (fd < 0) {
        printf("Error reading state\n");
        exit(-1);
    }
    read(fd, st, sizeof(struct shared_state));
    close(fd);
}

// 辅助函数：写入共享状态
void write_state(struct shared_state *st) {
    // O_TRUNC 会清空文件重新写，保证覆盖
    // 或者使用 O_RDWR 打开后覆盖
    // 这里为了简单，假设 open O_CREATE|O_RDWR 会覆盖开头
    int fd = open(SHM_FILE, O_RDWR); 
    if (fd < 0) {
        printf("Error writing state\n");
        exit(-1);
    }
    write(fd, st, sizeof(struct shared_state));
    close(fd);
}

void producer() {
    struct shared_state st;
    int item;
    
    for (int i = 0; i < NUM_ITEMS; i++) {
        item = i + 100;

        sem_wait(empty); // P(empty)
        sem_wait(mutex); // P(mutex) - 加锁，保证文件读写原子性

        // --- 临界区开始 ---
        read_state(&st); // 从文件读最新状态
        
        st.buffer[st.in] = item;
        printf("Prod [pid=%d] wrote %d at %d\n", getpid(), item, st.in);
        
        st.in = (st.in + 1) % BUFFER_SIZE;
        
        write_state(&st); // 写回文件
        // --- 临界区结束 ---

        sem_signal(mutex); // V(mutex)
        sem_signal(full);  // V(full)
    }
    exit(0);
}

void consumer() {
    struct shared_state st;
    int item;
    
    for (int i = 0; i < NUM_ITEMS; i++) {
        sem_wait(full);
        sem_wait(mutex);

        // --- 临界区开始 ---
        read_state(&st);
        
        item = st.buffer[st.out];
        printf("Cons [pid=%d] read %d from %d\n", getpid(), item, st.out);
        
        st.out = (st.out + 1) % BUFFER_SIZE;
        
        write_state(&st);
        // --- 临界区结束 ---

        sem_signal(mutex);
        sem_signal(empty);
    }
    exit(0);
}

int main() {
    // 1. 初始化文件
    int fd = open(SHM_FILE, O_CREATE | O_RDWR);
    struct shared_state init_st;
    memset(&init_st, 0, sizeof(init_st));
    write(fd, &init_st, sizeof(init_st));
    close(fd);

    // 2. 初始化信号量
    mutex = sem_open(1);
    empty = sem_open(BUFFER_SIZE);
    full = sem_open(0);

    if (mutex < 0 || empty < 0 || full < 0) {
        printf("Error: Failed to open semaphores.\n");
        return -1;
    }

    printf("Semaphores initialized: mutex=%d, empty=%d, full=%d\n", mutex, empty, full);

    int pid_consumer, pid_producer;

    // 2. 创建消费者进程
    pid_consumer = fork();
    if (pid_consumer < 0) {
        printf("Error: Fork consumer failed.\n");
        return -1;
    }

    if (pid_consumer == 0) {
        // 子进程：消费者
        consumer();
    }

    // 父进程
    // 3. 创建生产者进程
    pid_producer = fork();
    if (pid_producer < 0) {
        printf("Error: Fork producer failed.\n");
        // (理论上还应该处理已经创建的消费者进程)
        return -1;
    }

    if (pid_producer == 0) {
        // 子进程：生产者
        producer();
    }

    // 4. 父进程等待两个子进程结束
    printf("Main process waiting for children...\n");
    wait(0); // 等待一个子进程
    wait(0); // 等待另一个子进程

    printf("Main process finished.\n");
    return 0;
}
