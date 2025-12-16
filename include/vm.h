#ifndef VM_H
#define VM_H

#include "types.h"
#include "riscv.h"

// vm.c
pagetable_t vmem_create_pagetable(void);

pte_t *vmem_walk_pte(pagetable_t pagetable, uint64 virtual_addr, int alloc);

int vmem_map_pagetable(pagetable_t pagetable, uint64 virtual_addr, uint64 physical_addr, int permission);

int vmem_unmap_pagetable(pagetable_t pagetable, uint64 virtual_addr, int do_free);

void vmem_free_pagetable(pagetable_t pagetable);

void vmem_init(void);

void vmem_enable_paging(void);

int vmem_user_copy(pagetable_t src_pt, pagetable_t dst_pt, uint64 size);

int vmem_stack_copy(pagetable_t src_pt, pagetable_t dst_pt);

int vmem_copyin(pagetable_t pagetable, char *dst_kernel, uint64 src_user, uint64 len);

int vmem_copyout(pagetable_t pagetable, uint64 dst_user, char *src_kernel, uint64 len);

int vmem_user_alloc(pagetable_t pagetable, uint64 old_size, uint64 new_size);

uint64 vmem_user_dealloc(pagetable_t pagetable, uint64 old_size, uint64 new_size);

uint64 vmem_walk_addr(pagetable_t pagetable, uint64 va);

void test_vmem_mapping(void);

void test_kernel_pagetable(void);

void test_post_paging(void);

void test_write_to_readonly(void);

#endif //VM_H
