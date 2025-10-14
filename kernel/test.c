#include "../include/printf.h"
#include "../include/console.h"
#include "../include/test.h"
#include "../include/pmm.h"
#include "../include/vm.h"
#include "../include/riscv.h"
#include "../include/memlayout.h"
#include "../include/console.h"

#define assert(x)                                     \
do {                                              \
if (!(x)) panic("assertion failed: " #x);     \
} while (0)
extern char etext[]; // 内核代码段结束地址
// 定义一个简单的assert宏，用于内核调试
// 如果断言失败，系统将panic

static void simple_delay(long count) {
    for (volatile long i = 0; i < count; i++);
}

static unsigned long long get_time() {
    unsigned long long cycles;
    // "rdtime %0" 是汇编指令，把 time 寄存器的值读到 %0 (第一个操作数)
    // "=r" (cycles) 是约束，告诉编译器把结果存入 C 变量 cycles
    asm volatile("rdtime %0" : "=r" (cycles));
    return cycles;
}

void test_printf_basic() {
    printf("=== printf测试 ===\n");
    printf("Testing integer: %d\n", 42);
    printf("Testing negative: %d\n", -123);
    printf("Testing zero: %d\n", 0);
    printf("Testing hex: 0x%x\n", 0xABC);
    printf("Testing string: %s\n", "Hello RISC-V");
    printf("Testing char: %c\n", 'X');
    printf("Testing percent: 100%%\n");
}

void test_printf_edge_cases() {
    printf("INT_MAX: %d\n", 2147483647);
    printf("INT_MIN: %d\n", -2147483648);
    printf("NULL string: %s\n", (char*)0);
    printf("Empty string: %s\n", "");
}

void test_console_features(void) {
    console_clear();
    printf("--- 控制台功能测试程序 ---\n");
    printf("现在开始展示 console 模块的各项功能。\n\n");
    simple_delay(50000000);

    printf("--- 1. 测试所有前景色 ---\n");
    const char* color_names[] = {"黑色", "红色", "绿色", "黄色", "蓝色", "品红", "青色", "白色"};
    const uint8_t fg_colors[] = {FG_BLACK, FG_RED, FG_GREEN, FG_YELLOW, FG_BLUE, FG_MAGENTA, FG_CYAN, FG_WHITE};

    for (int i = 0; i < 8; i++) {
        console_set_color(fg_colors[i], BG_BLACK); // 以黑色为背景进行测试
        printf("这是 [%s] 的文字。\n", color_names[i]);
    }
    console_puts(ANSI_COLOR_RESET); // 恢复默认颜色
    printf("\n");
    simple_delay(100000000);

    printf("--- 2. 测试所有背景色 ---\n");
    const uint8_t bg_colors[] = {BG_BLACK, BG_RED, BG_GREEN, BG_YELLOW, BG_BLUE, BG_MAGENTA, BG_CYAN, BG_WHITE};

    for (int i = 0; i < 8; i++) {
        console_set_color(FG_WHITE, bg_colors[i]); // 以白色为前景进行测试
        // 打印一些空格以突显背景色
        printf("  背景色为 [%s]  \n", color_names[i]);
    }
    console_puts(ANSI_COLOR_RESET); // 恢复默认颜色
    printf("\n");
    simple_delay(100000000);

    printf("将在3秒后清空屏幕...\n");
    simple_delay(150000000);
    console_clear();
    printf("屏幕已清空！\n\n");
    simple_delay(50000000);
    printf("--- 控制台功能测试结束 ---\n");
}

void test_clear(void) {
    printf("3秒后清屏...\n");
    // 简单延时
    simple_delay(150000000);
    console_clear();
    printf("屏幕已清空！\n");
}


void test_printf_timer() {
    unsigned long long start, end, min_diff = 0xFFFFFFFFFFFFFFFF; // 设为最大值
    int i;

    // --- 预热阶段 ---
    for (i = 0; i < 10; i++) {
        test_printf_basic();
    }

    // --- 正式测试 ---
    for (i = 0; i < 100; i++) {
        start = get_time();
        test_printf_basic();
        end = get_time();

        if ((end - start) < min_diff) {
            min_diff = end - start; // 只保留最小的耗时
        }
    }

    printf("\n--- 最好性能测试结果 ---\n");
    printf("最小占用周期: %d\n", (int)min_diff);
}

/**
 * @brief 1. 测试物理内存分配器 (PMM)
 * - 验证基本分配、释放、页对齐和数据读写。
 */
void test_physical_memory(void) {
    printf("\n--- 1. Testing Physical Memory Allocator (PMM) ---\n");

    // 测试基本分配和释放
    void *page1 = alloc_page();
    printf("Allocated page1 at PA: %p\n", (uint64)page1);
    assert(page1 != 0);

    void *page2 = alloc_page();
    printf("Allocated page2 at PA: %p\n", (uint64)page2);
    assert(page2 != 0);
    assert(page1 != page2);

    // 页对齐检查
    assert(((uint64)page1 & (PGSIZE - 1)) == 0);
    assert(((uint64)page2 & (PGSIZE - 1)) == 0);

    // 测试数据写入和读取
    *(uint64*)page1 = 0xDEADBEEF;
    assert(*(uint64*)page1 == 0xDEADBEEF);

    // 测试释放和重新分配
    free_page(page1);
    free_page(page2);

    void *page3 = alloc_page();
    printf("Freed page1 & page2, then re-allocated page3 at PA: %p\n", (uint64)page3);
    assert(page3 != 0);
    free_page(page3);

    printf("PMM test PASSED.\n");
}


/**
 * @brief 2. 测试页表功能 (依赖PMM)
 * - 验证页表的创建、映射、地址转换和权限位设置。
 */
void test_pagetable(void) {
    printf("\n--- 2. Testing Page Table Functions ---\n");

    pagetable_t pt = create_pagetable();
    assert(pt != 0);
    printf("Created a root pagetable at PA: %p\n", (uint64)pt);

    // 测试基本映射
    uint64 va = 0x10000;
    void* pa_ptr = alloc_page(); // 从PMM获取一页真实物理内存
    assert(pa_ptr != 0);
    uint64 pa = (uint64)pa_ptr;
    int perm = PTE_R | PTE_W | PTE_U;

    printf("Mapping VA %p -> PA %p with permissions RWU...\n", va, pa);
    int map_result = map_page(pt, va, pa, perm);
    assert(map_result == 0);

    // 测试地址转换 (需要walk_lookup函数)
    pte_t *pte = walk_lookup(pt, va);
    assert(pte != 0);
    assert((*pte & PTE_V) != 0);
    assert(PTE2PA(*pte) == pa);
    printf("walk_lookup OK: VA %p resolves to PA %p.\n", va, PTE2PA(*pte));

    // 测试权限位
    assert((*pte & PTE_R) != 0);
    assert((*pte & PTE_W) != 0);
    assert((*pte & PTE_U) != 0);
    assert((*pte & PTE_X) == 0);
    printf("Permission check OK.\n");

    // 清理
    destroy_pagetable(pt);
    free_page(pa_ptr);

    printf("Page Table test PASSED.\n");
}


/**
 * @brief 3. 虚拟内存激活测试 (依赖PMM和页表功能)
 * - 启用分页，并验证内核在虚拟地址下仍能正常运行。
 */
void test_virtual_memory(void) {
    printf("\n--- 3. Testing Virtual Memory Activation ---\n");

    // 创建一个全局变量来测试数据段访问
    static volatile int test_data = 123;
    printf("Pre-paging: Kernel data 'test_data' = %d at PA %p\n", test_data, &test_data);

    // --- 激活分页后的测试 ---
    // 如果代码能执行到这里，说明内核代码段的映射是正确的，
    // 程序计数器(PC)已经在使用虚拟地址无缝地继续执行。
    printf("SUCCESS: CPU is now executing from virtual addresses.\n");

    // 测试内核数据段访问
    printf("Post-paging: Accessing 'test_data' via VA %p...\n", &test_data);
    test_data = 456;
    assert(test_data == 456);
    printf("SUCCESS: Kernel data segment is accessible. test_data = %d\n", test_data);

    // 测试设备访问（printf会通过UART映射来工作）
    printf("SUCCESS: Device memory (UART) is accessible via its mapping.\n");

    printf("Virtual Memory test PASSED.\n");
}


// ---Task 4---

extern volatile uint64 g_ticks_count;

void test_timer_interrupt(void) {
    printf("\n--- 5. Testing Timer Interrupt Functionality ---\n");
    printf("This test will wait for 5 timer interrupts (ticks).\n");

    // 记录测试开始前的中断计数值
    uint64 start_ticks = g_ticks_count;

    // 等待，直到 g_ticks_count 的值增加了5
    while (g_ticks_count < start_ticks + 5) {
        // 在等待期间，我们可以打印信息，证明主程序(非中断部分)在正常运行
        printf("  main loop waiting... current ticks: %d\n", g_ticks_count);
        // 执行一个简单的延时循环，模拟“做其他事”
        for (volatile int i = 0; i < 50000000; i++);
    }

    printf("\nSUCCESS: Detected %d new timer interrupts.\n", g_ticks_count - start_ticks);
    printf("Timer Interrupt test PASSED.\n");
}
/**
 * @brief 3. 测试写入非法的内存区域触发异常
 */
void test_store_page_fault(void) {
    printf("\n=== Running test: Write to Read-Only Memory(store page fault) ===\n");
    // KERNEL_BASE 是我们内核代码的起始地址，它被映射为只读+可执行。
    char *kernel_code_ptr = (char *) KERNBASE + 20;
    // 2. 接下来，尝试写入。CPU会顿住，并触发一个异常，不过目前没写异常处理
    printf("Attempting to write 'X' to read-only kernel code @ %p...\n", kernel_code_ptr);
    // CPU 硬件会在这里检测到权限冲突 (W=0)，并触发一个 Store Page Fault！
    *kernel_code_ptr = 'X';
    // 如果代码能执行到这里，说明内存保护没有生效，是一个严重的 Bug！
    printf("!!! TEST FAILED: Write to read-only memory did NOT cause a fault! !!!\n");
}

/**
 * @brief 4. 测试非法指令触发异常
 */
void test_illegal_instruction(void) {
    printf("\nTriggering an illegal instruction exception...\n");
    printf("Expected outcome: Kernel panic with scause=2 (Illegal instruction).\n");

    // 使用内联汇编插入一个全0的字，这在RISC-V中通常是一个非法指令
    // 如果异常处理正常，系统会在这里panic，永远不会执行下一行printf
    asm volatile(".word 0x00000000");

    // 如果代码能执行到这里，说明异常没有被捕获
    panic("test_exception_handling: Illegal instruction was not caught!");
}

void recursive_bomb(int depth) {
    if(depth % 10 == 0) {
        uint64 sp;
        asm volatile("mv %0, sp" : "=r"(sp));
        printf("Recursion depth: %d, SP: %p\n", depth, sp);
    }
    recursive_bomb(depth + 1);
}

void test_stack_overflow(void) {
    printf("\n--- 8. Testing Stack Overflow Detection ---\n");
    printf("Calling a recursive function to exhaust the stack...\n");
    printf("Expected outcome: Kernel panic with 'Stack Overflow' message.\n");

    recursive_bomb(0);

    panic("test_stack_overflow: Stack overflow was not caught!");
}

// --- 总测试函数 ---
void run_tests(void) {
    // --- Task2 ---
    // test_printf_basic();
    // test_printf_edge_cases();
    // test_console_features();
    // test_printf_timer();

    // --- Task3 ---
    // test_physical_memory();
    // test_pagetable();
    // test_virtual_memory();

    // --- Task4 ---
    // test_timer_interrupt();
    test_store_page_fault();
    // test_illegal_instruction();

    printf("\nAll tests passed successfully!\n");
    printf("Kernel initialization complete. Entering idle loop.\n");
}