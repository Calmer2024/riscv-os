#ifndef __USER_H__
#define __USER_H__

// 系统调用
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int kill(int);
int getpid(void);
int open(const char*, int);
int close(int);
int read(int, void*, int);
int write(int, const void*, int);
void* sbrk(int);

// ... 其他用户库函数 (如 printf)

#endif