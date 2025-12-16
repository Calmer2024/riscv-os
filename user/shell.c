//
// Created by czh on 2025/12/12.
//

#include "ulib/user.h"
#include "param.h"

// 解析命令行参数
// 例如: "ls -l" -> args[0]="ls", args[1]="-l", args[2]=0
int getcmd(char *buf, int nbuf) {
    printf("$ "); // 提示符
    memset(buf, 0, nbuf);

    // 从 stdin (fd 0) 读取一行输入
    // 因为 console_read 是行缓冲的，这里会阻塞直到用户按回车
    gets(buf, nbuf);

    if(buf[0] == 0) // EOF
        return -1;

    return 0;
}

// 把 buf 原地切成 argv，返回 argc
// 支持：多个空格/Tab；简单的单引号/双引号包裹参数（不支持转义）
static int parse_argv(char *buf, char *argv[], int maxargs) {
    int argc = 0;
    char *p = buf;

    while (*p) {
        // 跳过空白
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0) break;

        if (argc >= maxargs - 1) break; // 留一个给 0 结尾

        // 处理引号参数
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            argv[argc++] = p;
            while (*p && *p != quote) p++;
            if (*p == quote) *p++ = 0; // 截断
        } else {
            // 普通参数
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = 0; // 截断
        }
    }

    argv[argc] = 0;
    return argc;
}

int main(void) {
    static char buf[100];
    char *argv[MAXARG];
    int fd;

    // 确保有三个标准文件描述符
    // 如果 sh 是被 init 启动的，init 应该已经打开了 console
    while ((fd = open("console", O_RDWR)) >= 0) {
        if (fd >= 3) { close(fd); break; }
    }

    printf("Welcome to My Tiny Shell!\n");

    // 主循环
    while (getcmd(buf, sizeof(buf)) >= 0) {
        int len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = 0;
        if (buf[0] == 0) continue;

        int argc = parse_argv(buf, argv, MAXARG);
        if (argc == 0) continue;

        // 内置 cd：在父进程里切换，子进程无法影响当前 shell
        if (strcmp(argv[0], "cd") == 0) {
            if (argc < 2) {
                printf("cd: missing path\n");
                continue;
            }
            if (chdir(argv[1]) < 0) {
                printf("cd: cannot cd to %s\n", argv[1]);
            }
            continue;
        }
        // Fork 一个子进程去跑命令
        if (fork() == 0) {
            exec(argv[0], argv);
            // 如果 exec 返回了，说明失败了
            printf("exec %s failed\n", argv[0]);
            exit(0);
        }
        wait(0);
    }
    return 0;
}
