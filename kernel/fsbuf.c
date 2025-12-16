#include "../include/fs.h"
#include "../include/param.h"
#include "../include/printf.h"

// dev 在简化版本里面暂时用不上

struct {
    // buf缓存块
    // 以后访问时会不断把“刚用过的 buf”移到 head.next，形成真正的 LRU。
    struct fsbuf buf[FSBUF_NUM];
    // head 是哑元节点，不存真正数据，只是为了做好一个“永远不空”的链表。
    struct fsbuf head;
} fsbuf_cache;

void fsbuf_init(void) {
    struct fsbuf *b;
    // 用头插法把 buf[] 全挂到 head 后面，
    // 最后是 head <-> buf[N-1] <-> ... <-> buf[0] <-> head。
    fsbuf_cache.head.prev = &fsbuf_cache.head;
    fsbuf_cache.head.next = &fsbuf_cache.head;
    for (b = fsbuf_cache.buf; b < fsbuf_cache.buf + FSBUF_NUM; b++) {
        b->valid = 0;
        b->refcnt = 0;
        b->blockno = 0;
        b->dev = 0;
        sleeplock_init(&b->lock, "fsbuf");
        b->next = fsbuf_cache.head.next;
        b->prev = &fsbuf_cache.head;
        fsbuf_cache.head.next->prev = b;
        fsbuf_cache.head.next = b;
    }
    printf("fsbuf_init: %d buffers initialized.\n", FSBUF_NUM);
}

// 获取一个缓存块，不从磁盘读取数据
// 1. 如果缓存命中，refcnt++，返回
// 2. 如果未命中，回收一个 refcnt==0 的块
static struct fsbuf *fsbuf_get(uint dev, uint blockno) {
    struct fsbuf *b;

    // 1. 检查是否在缓存中 (Cache Hit?)
    for (b = fsbuf_cache.head.next; b != &fsbuf_cache.head; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            // printf("fsbuf(dev: %d, blockbo: %d) cached!\n", dev, blockno);
            b->refcnt++;
            return b;
        }
    }

    // 2. 未命中，需要分配新块 (Cache Miss)
    // 从后往前找 (prev方向)，找最久没用的块 (LRU)
    for (b = fsbuf_cache.head.prev; b != &fsbuf_cache.head; b = b->prev) {
        if (b->refcnt == 0) {
            // 找到了被替换块
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0; // 新块，数据还未读取
            b->refcnt = 1;
            return b;
        }
    }

    panic("fsbuf_get: no buffers"); // 缓存耗尽
    return 0;
}

// 读一个磁盘块：返回该块对应的缓存，保证 data 已经是最新的
struct fsbuf *fsbuf_read(uint dev, uint blockno) {
    struct fsbuf *b;
    b = fsbuf_get(dev, blockno);
    sleeplock_acquire(&b->lock);
    if (!b->valid) {
        // 还没读过，去驱动读=
        virtio_disk_rw(b, 0);
        b->valid = 1;
    }
    return b;
}

// 写回一个块（缓存到磁盘）
void fsbuf_write(struct fsbuf *b) {
    virtio_disk_rw(b, 1);
}

// 用完这个 fsbuf，降低引用计数，让它可以被 LRU 回收
void fsbuf_release(struct fsbuf *b) {
    sleeplock_release(&b->lock);
    b->refcnt--;
    if (b->refcnt < 0) {
        panic("fsbuf_release: refcnt <= 0");
    }

    if (b->refcnt == 0) {
        // 如果没人用了，把它移到链表头部 (head.next)
        // 表示它是“最近刚用过” (Most Recently Used)
        // 这样 LRU 算法就会最后才回收它

        // 1. 把 b 从当前位置摘出来
        b->next->prev = b->prev;
        b->prev->next = b->next;

        // 2. 插到 head 后面
        b->next = fsbuf_cache.head.next;
        b->prev = &fsbuf_cache.head;
        fsbuf_cache.head.next->prev = b;
        fsbuf_cache.head.next = b;
    }
}

// 简单打印当前链表顺序
void fsbuf_dump_list(void) {
    struct fsbuf *b;
    printf("fsbuf list: ");
    for (b = fsbuf_cache.head.next; b != &fsbuf_cache.head; b = b->next) {
        printf("(%u, ref=%d) -> ", b->blockno, b->refcnt);
    }
    printf("HEAD\n");
}

// 缓冲区测试
void fsbuf_test(void) {
    printf("=== fsbuf test begin ===\n");

    printf("--- [TEST] Start Bio Test ---\n");

    struct fsbuf *b = fsbuf_read(1, 2); // 读第2块
    printf("Read block 2: %x %x\n", b->data[0], b->data[1]);

    // 修改数据
    b->data[0] = 0xAA;
    b->data[1] = 0xBB;

    // 写回
    printf("Writing to block 2...\n");
    fslog_op_begin();
    fslog_write(b);
    fslog_op_end();

    // 释放
    fsbuf_release(b);

    // 再次读取（应该缓存命中，或者从磁盘读回修改后的数据）
    struct fsbuf *b2 = fsbuf_read(1, 2);
    printf("Read block 2 again: %x %x\n", b2->data[0], b2->data[1]);

    if (b2->data[0] == 0xAA && b2->data[1] == 0xBB) {
        printf("✅ [SUCCESS] Bio Cache Test Passed!\n");
    } else {
        printf("DEBUG: data[0] = %x, data[1] = %x\n", b2->data[0], b2->data[1]);
        panic("❌ [FAIL] Bio Cache Test Failed");
    }
    fsbuf_release(b2);
}

void fsbuf_test_lru(void) {
    // 需要把FSBUF_NUM 改成 4
    struct fsbuf *a, *b, *c, *d, *e;

    // 顺序读 0,1,2,3，再释放
    a = fsbuf_read(1, 0);
    fsbuf_release(a);
    b = fsbuf_read(1, 1);
    fsbuf_release(b);
    c = fsbuf_read(1, 2);
    fsbuf_release(c);
    d = fsbuf_read(1, 3);
    fsbuf_release(d);

    printf("after reading 0..3:\n");
    fsbuf_dump_list();

    // 此时链表从 head.next 到尾是：
    // 最近用过：3,2,1,0
    // 然后读 block 4，会触发一次 LRU 回收：
    e = fsbuf_read(1, 4); // 需要从尾部找 refcnt==0 的 buf

    printf("after reading 4:\n");
    printf("e=%p blockno=%u ref=%d\n",
           e, e->blockno, e->refcnt);
    fsbuf_dump_list();

    // 此时释放，4会到头部去
    fsbuf_release(e);
    printf("after release 4:\n");
    fsbuf_dump_list();
}
