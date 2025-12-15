#ifndef RISCV_OS_DEFS_H
#define RISCV_OS_DEFS_H
#include "types.h"

// syscall.c
void            argint(int, int*);
int             argstr(int, char*, int);
void            argaddr(int, uint64 *);
int             fetchstr(uint64, char*, int);
int             fetchaddr(uint64, uint64*);
void            syscall();

#endif //RISCV_OS_DEFS_H