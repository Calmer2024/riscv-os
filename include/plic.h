#ifndef PLIC_H
#define PLIC_H

// plic.c
void plic_init(void);
int plic_claim(void);
void plic_complete(int irq);

#endif //PLIC_H
