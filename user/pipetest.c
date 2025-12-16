//
// Created by czh on 2025/12/15.
//

#include "ulib/user.h"

// 一个模拟耗时工作的延迟循环
void delay(void) {
    volatile int i = 0;
    while (i < 500000000) {
        i++;
    }
}

int main() {
    int p[2];
    char buf[100];

    // 1. 创建管道
    // p[0] 是读端, p[1] 是写端
    if (pipe(p) < 0) {
        printf("pipe failed\n");
        exit(1);
    }

    int pid = fork();

    if (pid == 0) {
        // --- 子进程 (Writer) ---
        close(p[0]); // 关掉读端

        printf("Child: writing 'Hello' to pipe...\n");
        // delay();
        write(p[1], "Hello World", 12);

        close(p[1]); // 写完关闭
        exit(0);
    } else {
        // --- 父进程 (Reader) ---
        close(p[1]); // 关掉写端 (非常重要！否则 read 不会返回 0/EOF)

        printf("Parent: reading from pipe...\n");
        int n = read(p[0], buf, sizeof(buf));

        printf("Parent: received %d bytes: '%s'\n", n, buf);

        close(p[0]);
        wait(0);
    }
    exit(0);
}