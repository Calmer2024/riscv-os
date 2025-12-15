#include "../include/types.h"
#include "../include/memlayout.h"
#include "../include/riscv.h"
#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/printf.h"

extern char end[];

// 使用嵌入式链表管理空闲物理页
// 利用页本身的空间存储链表指针，每个空闲的前8字节存储下一个空闲页地址
typedef struct PageNode {
    struct PageNode *next;
} PageNode;

// 管理器的全局状态
static struct {
    //空闲链表头指针
    PageNode *free_list;
} pmm;

void pmm_init(void) {
    printf("pmm: initializing...\n");
    pmm.free_list = 0;

    // 从内核末尾开始，逐页释放所有可用物理内存
    char *p = (char*)PGROUNDUP((uint64)end);
    for(; p + PAGE_SIZE <= (char*)PHYSTOP; p += PAGE_SIZE) {
        free_page(p);
    }
    printf("pmm: initialization complete.\n");
}

void free_page(void* page) {
    // 页对齐检查|不能释放内核区域|不能超出物理内存
    if(((uint64)page % PAGE_SIZE) != 0 || (char*)page < end || (uint64)page >= PHYSTOP) {
        panic("free_page");
    }

    // 清空页面内容，并将其加入空闲链表头部
    // memset(page, 1, PAGE_SIZE); // 用非0值填充便于调试
    PageNode *p = (PageNode*)page;
    p->next = pmm.free_list;
    pmm.free_list = p;
}

void* alloc_page(void) {
    if (!pmm.free_list) {
        return 0; // 内存耗尽
    }
    
    // 从链表头部取出一个页
    PageNode *p = pmm.free_list;
    pmm.free_list = p->next;

    // 返回前将页面清零
    memset(p, 0, PAGE_SIZE);
    return (void*)p;
}