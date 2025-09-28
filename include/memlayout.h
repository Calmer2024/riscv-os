#ifndef __MEMLAYOUT_H__
#define __MEMLAYOUT_H__

// 页大小 (4KB)
#define PGSIZE 4096 

// 将地址向上或向下对齐到页边界
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

// 物理内存顶部地址 (假设为128MB)
#define PHYSTOP 0x88000000 // 0x80000000 + 128MB

// 内核加载的基地址
#define KERNBASE 0x80000000

// end 符号由链接脚本 kernel.ld 提供，代表内核代码+数据的末尾
extern char end[];

#endif