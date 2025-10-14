#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "proc.h" // 需要 trapframe 结构体

// 系统调用处理函数
void syscall_handler(struct trapframe *tf);

#endif