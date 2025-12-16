//
// Created by czh on 2025/10/21.
//

#include "fs.h"
#include "ulib/user.h"


// 一个模拟耗时工作的延迟循环
void delay(void) {
    volatile int i = 0;
    while (i < 50000000) {
        i++;
    }
}

// 递归测试栈能否正常工作
void deep_recursion(char *str, int n) {
    if (n > 0) {
        printf("%s: 递归剩余次数: %d\n", str, n);
        delay();
        int next = n - 1;
        deep_recursion(str, next);
    }
}

// 受到锁保护的临界区
void critical_section(char *name, int sem_id) {
    printf("临界区：%s (pid %d): 正在尝试获取锁\n", name, getpid());
    // 如果锁被占用, 进程会在这里 sleep
    sem_wait(sem_id);
    // 进入临界区
    printf("临界区：%s (pid %d): 成功获取锁\n", name, getpid());
    for (int i = 0; i < 5; i++) {
        printf("%s: %d\n", name, i);
        delay();
    }
    printf("临界区：%s (pid %d): 释放锁\n", name, getpid());
    // 退出临界区
    // 这会唤醒另一个正在 sleep 的进程
    sem_signal(sem_id);
}

// 信号量测试
int sem_test(void) {
    // 创建一个初始值为 1 的信号量 (当作互斥锁)
    printf("=== 信号量测试 ===");
    int sem_id = sem_open(1);
    if (sem_id < 0) {
        printf("sem_open 失败!\n");
        return -1;
    }
    printf("主进程：信号量创建成功, id = %d\n", sem_id);

    // 创建子进程
    int pid = fork();
    if (pid < 0) {
        printf("fork 失败!\n");
        return -1;
    }
    if (pid == 0) {
        // 子进程
        critical_section("子进程", sem_id);
        printf("子进程：任务完成，退出。\n");
        exit(0);
    } else {
        critical_section("父进程", sem_id);
        // 等待子进程退出
        int status;
        int exit_pid = wait(&status);
        printf("父进程：子进程(pid: %d, status: %d)已退出\n", exit_pid, status);
        return 0;
    }
}

// fork，wait，exit测试
int fork_test(void) {
    write(1, "\n", 1);
    printf("=== fork，wait，exit测试 ===\n");
    int pid = fork();
    if (pid < 0) {
        printf("fork failed!\n");
        return -1;
    }
    if (pid == 0) {
        // 子进程
        printf("这是子进程, pid = %d\n", getpid());
        deep_recursion("子进程", 20);
        printf("子进程结束\n");
        exit(0);
    } else {
        // 父进程
        printf("这是父进程, pid = %d\n", getpid());
        deep_recursion("父进程", 10);
        int status;
        printf("父进程等待子进程退出.\n");
        // 等待子进程退出
        int exit_pid = wait(&status);
        printf("子进程 (pid: %d) 退出, 状态码: %d\n", exit_pid, status);
        deep_recursion("父进程", 10);
        printf("父进程结束\n");
        return 0;
    }
}

#define NUM_CHILDREN 4

// fork与信号量测试
int fork_and_sem_test() {
    printf("=== 多进程同步测试 ===\n");
    // 创建一个初始值为 1 的信号量当作互斥锁
    int sem_id = sem_open(1);
    if (sem_id < 0) {
        printf("主进程: sem_open 失败!\n");
        return -1; // exit(-1)
    }
    printf("主进程 (pid %d): 信号量创建成功, id = %d\n", getpid(), sem_id);
    // 2. 循环创建 4 个子进程
    for (int i = 0; i < NUM_CHILDREN; i++) {
        int pid = fork();
        if (pid < 0) {
            printf("主进程: fork 第 %d 个子进程失败!\n", i);
        } else if (pid == 0) {
            // 子进程代码
            printf("子进程 %d (pid %d): 已创建\n", i, getpid());
            // 所有子进程都去执行临界区
            char name[16];
            sprintf(name, "子进程 %d", i);
            critical_section(name, sem_id);
            printf("子进程 %d (pid %d): 任务完成，退出。\n", i, getpid());
            return 0; // exit(0)
        }
    }
    // 父进程等待所有 4 个孩子退出
    printf("主进程 (pid %d): 已创建 %d 个孩子, 现在开始等待...\n", getpid(), NUM_CHILDREN);
    for (int i = 0; i < NUM_CHILDREN; i++) {
        int status;
        int exit_pid = wait(&status); //
        if (exit_pid < 0) {
            printf("主进程: wait 失败!\n");
        } else {
            printf("主进程: 回收了子进程 (pid: %d)，退出状态: %d\n", exit_pid, status);
        }
    }
    printf("=== 所有子进程已回收，测试通过 ===\n");
    return 0; // initproc 退出 (引发 panic)
}

// exit后继续fork测试
int fork_after_exit_test() {
    printf("=== exit后再次fork测试操作系统资源回收 ===");
    int pid = fork();
    if (pid < 0) {
        printf("fork filed\n");
        return -1;
    }
    if (pid == 0) {
        // 一个子进程
        printf("子进程: pid = %d\n", getpid());
        return 0;
    } else {
        // 父进程
        int status;
        int pid1 = wait(&status);
        printf("父进程: 收到了子进程 (pid: %d) 的退出状态: %d\n", pid1, status);
        printf("再fork一个\n");
        pid = fork();
        if (pid == 0) {
            printf("ok哈哈哈\n");
            return 0;
        } else {
            int pid2 = wait(&status);
            printf("父进程: 收到了子进程 (pid: %d) 的退出状态: %d\n", pid2, status);
            return 0;
        }
    }
    return 0;
}

#define PAGE_SIZE 0x1000

// sbrk测试，先要修改trap_user中处理访存异常的方式，因为会触发一个访存异常
void sbrk_test(void) {
    printf("=== sbrk 测试开始 ===\n");
    // 获取初始断点 (堆顶)
    char *initial_break = sbrk(0);
    printf("初始断点 (堆顶) 在: %p\n", initial_break);
    // 尝试分配一页内存
    printf("调用 sbrk(%d) 分配一页...\n", PAGE_SIZE);
    char *allocated_mem = sbrk(PAGE_SIZE);
    if (allocated_mem == (char *) -1) {
        printf("sbrk 分配失败!\n");
        return;
    }
    // 验证返回值
    if (allocated_mem != initial_break) {
        printf("sbrk 返回值错误！期望 %p, 得到 %p\n", initial_break, allocated_mem);
        return; // 测试失败
    }
    printf("sbrk 返回了正确的旧断点: %p\n", allocated_mem);
    // 获取新的断点，验证堆是否增长
    char *new_break = sbrk(0);
    printf("新的断点在: %p\n", new_break);
    if (new_break != initial_break + PAGE_SIZE) {
        printf("sbrk 增长错误！期望 %p, 得到 %p\n", initial_break + PAGE_SIZE, new_break);
        return;
    }
    printf("堆成功增长了 %d 字节\n", PAGE_SIZE);
    // 尝试写入新分配的内存
    printf("尝试写入新分配内存的第一个字节 (%p)\n", allocated_mem);
    *allocated_mem = 'H';
    printf("尝试读取\n");
    if (*allocated_mem == 'H') {
        printf("写入和读取成功\n");
    } else {
        printf("读取错误！内存可能未正确映射或不可写\n");
        return;
    }
    // 故意越界，应该触发usertrap，暂时不处理，正常返回
    printf("故意越界写入，触发trap_user\n");
    allocated_mem[PAGE_SIZE + 1] = 'Z';

    //释放刚刚分配的内存
    printf("调用 sbrk(%d) 释放内存...\n", -PAGE_SIZE);
    char *dealloc_ret = sbrk(-PAGE_SIZE);

    if (dealloc_ret == (char *) -1) {
        printf("sbrk 释放失败!\n");
        return; // 测试失败
    }

    // 7. 验证 sbrk(-n) 的返回值
    if (dealloc_ret != new_break) {
        printf("sbrk(-n) 返回值错误！期望 %p, 得到 %p\n", new_break, dealloc_ret);
        return; // 测试失败
    }
    printf("sbrk(-n) 返回了正确的旧断点: %p\n", dealloc_ret);

    // 获取最终断点，验证是否回到原位
    char *final_break = sbrk(0);
    printf("最终断点在: %p\n", final_break);
    if (final_break != initial_break) {
        printf("sbrk 释放错误！期望 %p, 得到 %p\n", initial_break, final_break);
        return;
    }
    printf("堆成功收缩回初始大小。\n");
    // 故意再写入触发usertrap
    printf("故意越界写入，触发trap_user\n");
    allocated_mem[0] = 'Z';
    printf("=== sbrk 测试通过 ===\n");
}



int main(void) {
    printf("Usertest Start.\n");
    sem_test();
    // fork_test();
    // fork_and_sem_test();
    // fork_after_exit_test();
    // sbrk_test();
    printf("test passed!\n");
}