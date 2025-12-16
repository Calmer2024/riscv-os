#include "../include/fs.h"
#include "../include/printf.h"
#include "../include/string.h"

// 内存中的日志头副本
struct fslog_header log_header;
// 日志锁
struct sleeplock log_lock;

// 标记日志在磁盘的什么位置
uint log_start_block;

// 测试专用全局变量
int FSLOG_TEST_CRASH = 0; // 0:正常, 1:写日志区时崩, 2:写完Header后崩(测恢复)


// 将内存中的 log_header 写入磁盘日志区的第一个块
static void fslog_header_write() {
    struct fsbuf *bp = fsbuf_read(ROOTDEV, log_start_block);
    struct fslog_header *hb = (struct fslog_header *) (bp->data);

    hb->n = log_header.n;
    for (int i = 0; i < log_header.n; i++) {
        hb->block_nums[i] = log_header.block_nums[i];
    }

    fsbuf_write(bp); // 真正的写盘操作（bio.c 里的 bwrite）
    fsbuf_release(bp);
}

// Install，把日志区的数据拷贝回数据块
static void fslog_install_trans() {
    for (int i = 0; i < log_header.n; i++) {
        // 1. 读日志块 (Log Block)
        struct fsbuf *lbuf = fsbuf_read(ROOTDEV, log_start_block + i + 1);
        // 2. 读目标块 (Home Block)
        struct fsbuf *dbuf = fsbuf_read(ROOTDEV, log_header.block_nums[i]);
        // 3. 内存拷贝
        memmove(dbuf->data, lbuf->data, BSIZE);
        // 4. 写回目标块
        fsbuf_write(dbuf);
        fsbuf_release(lbuf);
        fsbuf_release(dbuf);
    }
}

// 上层调用：把一个 buffer 加入当前事务（替代直接写盘）
void fslog_write(struct fsbuf *b) {
    if (log_header.n >= LOGBLOCKS) {
        panic("fslog: transaction too big"); // 直接简单粗暴报错，防止溢出
    }

    uint i = log_header.n;
    log_header.block_nums[i] = b->blockno; // 记录它本来是哪个块
    log_header.n++;

    // 把这个 buffer pin 住，不让 bio 层回收）
    b->refcnt++;
}

void fslog_copy_to_log(int i) {
    // 找到内存中缓存的那个 buffer
    struct fsbuf *b = fsbuf_read(ROOTDEV, log_header.block_nums[i]); // 需确保 bio 有这个查找函数

    // 读出日志区的目标块
    struct fsbuf *lbuf = fsbuf_read(ROOTDEV, log_start_block + i + 1);

    // 拷贝数据
    memmove(lbuf->data, b->data, BSIZE);

    fsbuf_write(lbuf); // 写日志区到磁盘

    fsbuf_release(lbuf);
    fsbuf_release(b); // 释放在 fslog_write 里增加的 refcnt
}

// 核心流程：提交事务
void fslog_commit() {
    if (log_header.n > 0) {
        // --- 模拟场景 A: 写日志写一半断电 ---
        if (FSLOG_TEST_CRASH == 1) {
            printf("TEST: Crashing during log write...\n");
            // 只写一半的日志块
            for (int i = 0; i < log_header.n / 2; i++) {
                fslog_copy_to_log(i);
            }
            panic("CRASH: Power failure during log write!");
        }

        // 步骤 1: 把所有被修改的 buffer 写入磁盘的日志区 (Write Log Blocks)
        for (int i = 0; i < log_header.n; i++) {
            fslog_copy_to_log(i);
        }

        // 步骤 2: 写日志头 - 保证是一整块操作的原子操作
        fslog_header_write();

        // --- 模拟场景 B: 刚 Commit 完，还没安装就断电 ---
        if (FSLOG_TEST_CRASH == 2) {
            panic("CRASH: Power failure after commit!");
        }

        // 步骤 3: 安装事务 (Install) - 把数据搬到真正的位置
        fslog_install_trans();

        // 步骤 4: 清除日志头 (Clean)
        log_header.n = 0;
        fslog_header_write();
    }
}

// 初始化，检查日志，进行恢复重做
void fslog_init(int dev, struct superblock *sb, int debug) {
    log_start_block = sb->logstart;
    sleeplock_init(&log_lock, "fslog"); // 初始化锁
    // 检查磁盘上的日志头
    struct fsbuf *bp = fsbuf_read(dev, log_start_block);
    struct fslog_header *hb = (struct fslog_header *) (bp->data);

    // 如果头里 n > 0，说明上次断电了，需要重放
    if (hb->n > 0) {
        if (debug) {
            printf("DEBUG: fslog_init: redo");
        }
        // 把磁盘上的头读到内存
        log_header.n = hb->n;
        for (int i = 0; i < hb->n; i++) log_header.block_nums[i] = hb->block_nums[i];

        // 执行重放 (Replay)
        fslog_install_trans();

        // 清空日志
        log_header.n = 0;
        fslog_header_write();
    }

    fsbuf_release(bp);
}

// 日志开始
void fslog_op_begin() {
    // 获取锁，如果其他进程拿着锁正在 sleep 等磁盘，走到这里会 sleep 等锁
    // 不支持开启多个事务
    sleeplock_acquire(&log_lock);
    if (log_header.n != 0)
        panic("fslog_op_begin: nesting not supported");
}

// 日志结束
void fslog_op_end() {
    fslog_commit(); // 立即提交
    // 释放锁
    sleeplock_release(&log_lock);
}
