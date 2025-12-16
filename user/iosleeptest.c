//
// Created by czh on 2025/12/15.
//

#include "ulib/user.h"

void delay(char *name, int speed) {
    int i = 0;
    while (i < 500000000 / speed) {
        i++;
        if (i % 100000000 == 0) {
            printf("%s: calculation %d\n", name, i);
        }
    }
}

int main() {
    int pid = fork();

    if (pid == 0) {
        // --- 子进程：CPU 密集型任务 ---
        // 它不涉及磁盘，只负责占用 CPU

        delay("Child", 1);
        exit(0);
    } else {
        // --- 父进程：IO 密集型任务 ---
        // 疯狂写磁盘，如果它是忙等待，它会霸占 CPU，子进程根本没机会输出
        // 如果它是 Sleep，它等磁盘时会把 CPU 让给子进程
        printf("Parent: Starting heavy write...\n");
        int fd = open("test_io", O_CREATE | O_RDWR);

        char buf[512];
        memset(buf, 'A', 512);

        // 写个几十次，制造足够长的 I/O 时间
        for (int i = 0; i < 10; i++) {
            printf("parent write\n");
            write(fd, buf, 512);
            delay("Parent", 10);
            // 如果 sleep 生效，每次 write 都会让出 CPU
        }

        close(fd);
        printf("Parent: Write done.\n");
        wait(0);
    }
    exit(0);
}
