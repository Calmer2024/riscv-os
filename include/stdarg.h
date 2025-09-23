#ifndef STDARG_H
#define STDARG_H

typedef __builtin_va_list va_list;  // 可以把va_list理解为参数的指针
#define va_start(v,l)  __builtin_va_start(v,l)  // 初始化 va_list 变量。它告诉 va_list 变量从哪里开始读取可变参数
#define va_end(v)      __builtin_va_end(v)      // 清理 va_list 变量。在函数返回之前，必须调用它来释放 va_start 可能分配的任何资源。这确保了程序的健壮性和可移植性。
#define va_arg(v,l)    __builtin_va_arg(v,l)    // 获取当前 va_list 指向的参数，并使 va_list 指向下一个参数；l是你期望获取的这个可变参数的类型（int等），va_arg 需要知道应该从内存中读取多少个字节，并如何解释这些字节。
#define va_copy(d,s)   __builtin_va_copy(d,s)   // 复制一个 va_list 的状态。有时你可能需要多次遍历可变参数列表，va_copy 可以让你保存当前遍历的位置，或者创建一个副本。

#endif