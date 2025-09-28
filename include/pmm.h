#ifndef __PMM_H__
#define __PMM_H__

#include "types.h"

void pmm_init(void);
void* alloc_page(void);
void free_page(void* page);

#endif