#ifndef __VM_H__
#define __VM_H__

#include "types.h"

// 页表类型定义
typedef uint64* pagetable_t;
typedef uint64 pte_t;
extern pagetable_t kernel_pagetable;

pagetable_t create_pagetable(void);
void destroy_pagetable(pagetable_t pt);
int map_page(pagetable_t pt, uint64 va, uint64 pa, int perm);
void dump_pagetable(pagetable_t pt);
pte_t* walk_lookup(pagetable_t pt, uint64 va);

// --- 内核页表初始化 ---
void kvminit(void);
void kvminithart(void);

#endif