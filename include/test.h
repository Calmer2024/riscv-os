#ifndef __TESTS_H__
#define __TESTS_H__

// 用于Task2的printf测试
void test_printf_basic(void);
void test_printf_edge_cases(void);
void test_console_features(void);
void test_console_features(void);
void test_clear(void);
unsigned long long get_time();
void test_printf_timer();

// 用于Task3的memory测试
void test_physical_memory(void);
void test_pagetable(void);
void test_virtual_memory(void);

#endif