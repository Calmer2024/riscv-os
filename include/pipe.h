#ifndef RISCV_OS_PIPE_H
#define RISCV_OS_PIPE_H

#include "file.h"

// pipe.c
int pipe_alloc(struct file **f0, struct file **f1);

void pipe_close(struct pipe *pi, int writeable);

int pipe_read(struct pipe *pi, uint64 addr, int n);

int pipe_write(struct pipe *pi, uint64 addr, int n);
#endif //RISCV_OS_PIPE_H
