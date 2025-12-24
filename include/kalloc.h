#ifndef KALLOC_H
#define KALLOC_H

void kmem_init(void);
void kmem_free(void *phys_addr);
void kmem_dump(void);
void *kmem_alloc(void);

#endif //KALLOC_H
