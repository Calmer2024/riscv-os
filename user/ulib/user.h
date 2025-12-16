//
// Created by czh on 2025/10/22.
//
#ifndef RISCV_OS_USER_H
#define RISCV_OS_USER_H

#include "../include/sysinfo.h"

// syscall
int getpid(void);

int fork(void);

int wait(int *);

int exit(int status) __attribute__((noreturn));

int exec(const char *, char **); // path, argv

#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400
// 打开文件
// path: 文件路径
// omode: 打开模式 (O_RDONLY, O_WRONLY, O_RDWR, O_CREATE 等)
// 返回: 成功返回文件描述符(fd)，失败返回 -1
int open(const char *path, int omode);

// 关闭文件
// fd: 要关闭的文件描述符
// 返回: 成功返回 0，失败返回 -1
int close(int fd);

// 读取文件
// fd: 文件描述符
// buf: 接收数据的缓冲区 (注意这里不是 const，因为要往里写数据)
// n: 试图读取的字节数
// 返回: 实际读取的字节数，0 表示文件结束(EOF)，-1 表示出错
int read(int fd, void *buf, int n);

// 写入文件
// fd: 文件描述符
// buf: 要写入的数据缓冲区 (这里是 const，因为只读不改)
// n: 写入字节数
// 返回: 实际写入的字节数，-1 表示出错
int write(int fd, const void *buf, int n);

int fstat(int fd, struct stat *);

int mkdir(const char *);

int sysinfo(struct sysinfo *);

int sem_open(int init_val);

int sem_wait(int sem_id);

int sem_signal(int sem_id);

void *sbrk(int size);

void fslog_crash(int type);

int chdir(const char *path);

int pipe(int fd[2]);

// 硬链接与时间相关的系统调用
int link(const char *oldpath, const char *newpath);

int unlink(const char *path);

int sleep(int n);

int uptime(void);

// uprintf.c
int printf(const char *fmt, ...);

// string.c
int sprintf(char *out, const char *fmt, ...);

void *memset(void *dst, int c, uint n);

void *memmove(void *vdst, const void *vsrc, int n);

uint strlen(const char *s);

int strcmp(const char *p, const char *q);

char *strcpy(char *s, const char *t);

char *gets(char *buf, int max);

int atoi(const char *s);

// ulib.c
int stat(const char *n, struct stat *st);


#endif //RISCV_OS_USER_H
