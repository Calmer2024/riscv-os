#ifndef __VM_H__
#define __VM_H__

#include "types.h"

struct trapframe;

// 页表类型定义
typedef uint64* pagetable_t;
typedef uint64 pte_t;
extern pagetable_t kernel_pagetable;

// --- 核心映射函数 ---

pagetable_t kvmmake(void); // 创建内核页表
void kvminit(void);
void kvminithart(void);
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
pte_t* walk(pagetable_t pagetable, uint64 va, int alloc);

// --- 用户虚拟内存管理 (UVM) ---
pagetable_t uvmcreate(void);
void uvminit(pagetable_t pagetable, uchar *src, uint sz);
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm);
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz);
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz);
void uvmfree(pagetable_t pagetable, uint64 sz);
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free);

// --- 内核/用户数据拷贝 ---
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len);
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len);
int copy_from_user(void *kernel_dst, uint64 user_src, uint64 len);
int copy_to_user(uint64 user_dst, void *kernel_src, uint64 len);
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max);

// --- 调试与异常处理 ---
void dump_pagetable(pagetable_t pt);
void handle_page_fault(struct trapframe *tf);
void destroy_pagetable(pagetable_t pt);

#endif