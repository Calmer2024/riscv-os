#include "../include/memlayout.h"
#include "../include/riscv.h"
#include "../include/types.h"
#include "../include/printf.h"
#include "../include/string.h"

// 空闲页节点，下一页的地址存放在当前空闲页的前8个字节中
struct node {
    struct node *next;
};

// 头节点
struct {
    struct node *head;
} freelist = {0};

// 链接器脚本 kernel.ld 提供的内核代码和数据的末尾地址
extern char end[];

void kmem_free(void *phys_addr) {
    struct node *mem_node;
    // 安全检查
    if ((uint64) phys_addr % PAGE_SIZE != 0)
        panic("kmem_free: misalignment address");
    if ((char *) phys_addr < (char *) PAGE_UP((uint64)end) || (uint64) phys_addr >= PHYS_TOP)
        panic("kmem_free: invalid address");
    // 填充垃圾数据，用于调试
    memset(phys_addr, 1, PAGE_SIZE);

    mem_node = (struct node *) phys_addr;

    // 头插法将页加入空闲链表
    mem_node->next = freelist.head;
    freelist.head = mem_node;
}

// 申请一页物理内存，返回页的起始地址
// 如果没有空闲页，则触发panic
void *kmem_alloc(void) {
    // 分配：从链表头摘下来一个节点，如果不是0，说明是有空闲的页
    struct node *mem_node;
    mem_node = freelist.head;
    if (mem_node) {
        freelist.head = mem_node->next;
        // 填充数据0
        memset((char *) mem_node, 0, PAGE_SIZE);
    } else {
        // 物理内存用完了
        panic("kmem_alloc: out of memory");
        return 0;
    }
    // 返回页的起始地址
    return mem_node;
}

// 传进来的参数很可能没对齐
static void freerange(void *physical_addr_start, void *physical_addr_end) {
    char *p = (char *) PAGE_UP((uint64)physical_addr_start);
    // p + PAGE_SIZE 的写法是确保整个页的范围都在界内
    for (; p + PAGE_SIZE <= (char *) physical_addr_end; p += PAGE_SIZE) {
        kmem_free(p);
    }
}

// 调试函数：打印空闲链表的状态
void kmem_dump(void) {
    struct node *mem_node;
    uint32 free_pages = 0;

    printf_color("=== Kmem Dump Start ===\n",PURPLE);

    // 1. 遍历整个空闲链表并计数
    for (mem_node = freelist.head; mem_node; mem_node = mem_node->next) {
        free_pages++;
    }
    printf("Total free pages: %d\n", free_pages);

    // 2. 打印链表最前面的几个节点的地址，看看结构
    printf("First 10 pages in freelist:\n");
    int count = 0;
    for (mem_node = freelist.head; mem_node && count < 10; mem_node = mem_node->next) {
        printf("  [%d] Page Addr: 0x%p\n", count, mem_node);
        count++;
    }

    printf_color("=== Kmem Dump End ===\n",PURPLE);
}

// 初始化物理内存管理器
void kmem_init(void) {
    freerange(end, (void *) PHYS_TOP);
    printf_color("kmem_init: free memory initialized.\n",BLACK);
}

// 尝试解引用非法地址，CPU会卡住，触发内存访问，跳转异常处理程序，
// 但是现在没写异常处理程序，会直接卡住
void test_write_to_nullptr(void) {
    printf_color("=== Running test: Null pointer dereference... ===\n",YELLOW);
    char *p = kmem_alloc(); // 要求kalloc返回一个0， p 现在是 NULL (地址 0x0)
    printf("Allocated pointer: %p\n", p);
    *p = 'A'; // 尝试向地址 0x0 写入一个字符
    printf("This message should NOT be printed.\n");
}
