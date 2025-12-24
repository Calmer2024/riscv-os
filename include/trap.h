#ifndef TRAP_H
#define TRAP_H

// trap.c
void trap_init(void);
void test_store_page_fault(void);
void trap_user_return();

#endif //TRAP_H
