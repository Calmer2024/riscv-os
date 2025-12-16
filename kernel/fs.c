#include "../include/fs.h"
#include "../include/param.h"
#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/string.h"
#include "../include/vm.h"

#define min(a, b) ((a) < (b) ? (a) : (b))


// 全局的超级块副本，读入后常驻内存
struct superblock sb;

// 读取超级块
static void fs_read_superblock(int dev) {
    struct fsbuf *bp;
    // 超级块永远位于磁盘的第 1 块 (第 0 块是引导块)
    bp = fsbuf_read(dev, 1);
    // 把 buffer 数据拷贝到结构体中
    memmove(&sb, bp->data, sizeof(sb));
    // 释放 buffer
    fsbuf_release(bp);
}

// 文件系统初始化：初始化块缓存，读取超级块并且校验
void fs_init(int dev, int debug) {
    fsbuf_init();
    fs_read_superblock(dev);
    fslog_init(dev, &sb, debug);
    // 2. 校验魔数
    if (sb.magic == FSMAGIC) {
        if (debug) {
            printf("fs_init: superblock loaded successfully.\n");
            printf("    magic: 0x%x\n", sb.magic);
            printf("    size: %d blocks\n", sb.size);
            printf("    inodes num: %d\n", sb.ninodes);
            printf("    inode start: block %d\n", sb.inodestart);
            printf("    bmap start: block %d\n", sb.bmapstart);
            printf("    log start: block %d (size: %d blocks)\n", sb.logstart, sb.nlog);
            printf("    data blocks: %d (starting from block %d)\n", sb.nblocks, sb.bmapstart + (sb.size / BPB + 1));
            // fs_test_bitmap(dev);
        }
    } else {
        printf("❌ [FS] Invalid magic number: 0x%x (expected 0x%x)\n", sb.magic, FSMAGIC);
        panic("fsinit: invalid filesystem");
    }
    printf("fs_init: file system initialized\n");
}

// ================== 数据块相关 =================

// 把磁盘上的某一整个块填 0
static void fs_block_zero(uint dev, uint blockno) {
    struct fsbuf *bp = fsbuf_read(dev, blockno);
    memset(bp->data, 0, BSIZE);
    fslog_write(bp);
    fsbuf_release(bp);
}

// 分配一个清0的磁盘块，在bitmap上标记，返回分配的块号
uint fs_block_alloc(uint dev) {
    // 遍历所有的 Bitmap 块 (通常就 1 个，大磁盘会有多个)。
    // 在每个 Bitmap 块里，遍历每一位 (0 ~ 8191)。
    // 找到第一个为 0 的位。
    // 算出对应的物理块号 blockno。
    // 将该位改为 1，写回 Bitmap。
    // 调用 fs_block_zero 清空该物理块。
    // 返回块号。
    struct fsbuf *bp;
    for (int b = 0; b < sb.size; b++) {
        // 读取当前范围对应的 bitmap 块
        bp = fsbuf_read(dev, BBLOCK(b, sb));
        for (int bi = 0; bi < BPB && b + bi < sb.size; bi++) {
            int byte_idx = bi / 8;
            int m = 1 << (bi % 8); // 生成掩码
            int is_used = bp->data[byte_idx] & m;
            // 检查该位是否为 0 (空闲)
            if (!is_used) {
                // 找到空闲的位，标记被占用
                bp->data[byte_idx] |= m;
                // 写回 Bitmap (持久化分配状态)
                fslog_write(bp);
                fsbuf_release(bp);
                // 算出块号
                uint blockno = b + bi;
                // 清零新块的内容
                fs_block_zero(dev, blockno);
                return blockno;
            }
        }
        // 当前 bitmap 块满了，释放它，继续找下一个 bitmap 块
        fsbuf_release(bp);
    }
    panic("fs_block_alloc: out of blocks");
    return 0;
}

// 释放一个磁盘块
void fs_block_free(uint dev, uint blockno) {
    struct fsbuf *bp;
    bp = fsbuf_read(dev, BBLOCK(blockno, sb));
    // 计算在当前 bitmap 块内的位偏移
    uint bi = blockno % BPB;
    // 生成掩码
    int m = 1 << (bi % 8);
    // 如果该位已经是 0，说明被重复释放了
    if ((bp->data[bi / 8] & m) == 0) {
        panic("fs_block_free: freeing free block");
    }
    // 将该位改为 0，写回 Bitmap
    bp->data[bi / 8] &= ~m;
    fslog_write(bp);
    fsbuf_release(bp);
}

// ================== Inode 相关 =================

// 全局的 Inode 缓存表
struct {
    // struct spinlock lock; // 单核简版暂时不用锁
    struct inode inode[NINODE];
} itable;

// 初始化 Inode 缓存表
void fs_inode_init() {
    // 锁初始化可以省略
    // 内存清零通常由 bss 段自动完成
    for (int i = 0; i < NINODE; i++) {
        itable.inode[i].ref = 0;
        itable.inode[i].valid = 0;
        sleeplock_init(&itable.inode[i].lock, "inode");
    }
}

void fs_inode_lock(struct inode *ip) {
    if (ip == 0 || ip->ref < 1) {
        panic("fs_inode_lock");
        return;
    }
    sleeplock_acquire(&ip->lock);
    // 获取锁之后，如果发现数据还没读进来，顺便读一下
    if (ip->valid == 0) {
        fs_inode_read(ip);
    }
}

void fs_inode_unlock(struct inode *ip) {
    if (ip == 0 || ip->ref < 0)
        panic("fs_inode_unlock");
    sleeplock_release(&ip->lock);
}

// 获取内存 inode (引用计数 +1)，相当于fsbuf_get()，只处理缓存相关的东西，真正从磁盘读取由read()执行
struct inode *fs_inode_get(uint dev, uint inum) {
    struct inode *ip, *empty;

    // 1. 先找找是不是已经在缓存里了
    empty = 0;
    for (int i = 0; i < NINODE; i++) {
        ip = &itable.inode[i];
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
            // 缓存命中
            ip->ref++;
            return ip;
        }
        // 顺手找一个空位
        if (empty == 0 && ip->ref == 0)
            empty = ip;
    }

    // 2. 没缓存，分配一个新槽位
    if (empty == 0) {
        // inode 缓存满了
        panic("fs_inode_get: no inodes");
        return 0;
    }
    ip = empty;
    ip->dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0; // 标记为无效，等 iread 时再读盘
    return ip;
}

// 从磁盘读取 inode 数据
void fs_inode_read(struct inode *ip) {
    struct fsbuf *bp;
    struct dinode *dip;

    if (ip == 0 || ip->ref < 1) {
        // 安全检查：ip 被引用且非空
        panic("fs_inode_read: invalid inode.");
        return;
    }

    // 如果数据已经有效，直接返回
    if (ip->valid)
        return;

    // 1. 算出 inode 在哪个磁盘块，读取出来
    uint block = IBLOCK(ip->inum, sb);
    bp = fsbuf_read(ip->dev, block);

    // 2. 算出在该块内的偏移，得到 dinode 指针
    dip = (struct dinode *) bp->data + (ip->inum % IPB);

    // 3. 拷贝数据 (磁盘 dinode -> 内存 inode)
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    // 释放缓冲区
    fsbuf_release(bp);
    ip->valid = 1; // 标记有效
    if (ip->type == 0)
        panic("fs_inode_read: inode has no type");
}

// 写回 inode 数据
void fs_inode_write(struct inode *ip) {
    struct fsbuf *bp;
    struct dinode *dip;

    // 算出 inode 在哪个磁盘块，读取出来，拿到 dinode 指针
    uint block = IBLOCK(ip->inum, sb);
    bp = fsbuf_read(ip->dev, block);
    dip = (struct dinode *) bp->data + (ip->inum % IPB);

    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));

    fslog_write(bp); // 写回磁盘
    fsbuf_release(bp); // 释放缓冲区
}

// 返回 inode ip 的第 bn 个逻辑块对应的磁盘物理块号
// 如果该块不存在，会分配它 (allocate)
uint fs_inode_map(struct inode *ip, uint bn) {
    uint addr, *a;
    struct fsbuf *bp;

    // 1. 直接块
    if (bn < NDIRECT) {
        if ((addr = ip->addrs[bn]) == 0) {
            // 如果还没分配，分配一个新块
            addr = fs_block_alloc(ip->dev);
            ip->addrs[bn] = addr;
            fs_inode_write(ip); // 更新 inode (因为 addrs 变了)
        }
        return addr;
    }

    // 2. 一级间接块
    bn -= NDIRECT; // 调整索引，0 对应间接块里的第 0 项

    if (bn < NINDIRECT) {
        // 2.1 检查间接块本身是否存在，没有就分配
        if ((addr = ip->addrs[NDIRECT]) == 0) {
            addr = fs_block_alloc(ip->dev);
            ip->addrs[NDIRECT] = addr;
            fs_inode_write(ip);
        }

        // 2.2 读取间接块的内容
        bp = fsbuf_read(ip->dev, addr);
        a = (uint *) bp->data; // 当作数组

        // 2.3 检查目标数据块是否存在，没有就分配
        if ((addr = a[bn]) == 0) {
            addr = fs_block_alloc(ip->dev);
            a[bn] = addr;
            fslog_write(bp); // 间接块内容变了，写回
        }
        fsbuf_release(bp); // 记得释放
        return addr;
    }

    panic("fs_inode_map: out of range");
    return 0;
}

// 分配一个新的磁盘 inode，返回内存inode
struct inode *fs_inode_alloc(uint dev, short type) {
    int inum;
    struct fsbuf *bp;
    struct dinode *dip;

    // 遍历所有 inode (从 1 开始)
    for (inum = 1; inum < sb.ninodes; inum++) {
        bp = fsbuf_read(dev, IBLOCK(inum, sb)); // 拿到这个inode所在的磁盘块
        dip = (struct dinode *) bp->data + (inum % IPB); // 拿到 dinode 指针（理解为dinode数组）

        if (dip->type == 0) {
            // 找到了空闲位，清空初始化
            memset(dip, 0, sizeof(*dip));
            dip->type = type; // 标记为已占用
            fslog_write(bp); // 标记占据，写回释放
            fsbuf_release(bp);

            struct inode *ip = fs_inode_get(dev, inum);
            fs_inode_lock(ip);
            ip->valid = 1;
            ip->type = type;
            ip->major = 0;
            ip->minor = 0;
            ip->size = 0;
            ip->nlink = 0;
            memset(ip->addrs, 0, sizeof(uint) * ((NDIRECT + 1)));
            fs_inode_unlock(ip);
            return ip; // 返回内存 inode
        }
        fsbuf_release(bp);
    }
    panic("fs_inode_alloc: no inodes available");
    return 0;
}

// 将 inode 占用的所有数据块释放，并将大小设为 0
void fs_inode_trunc(struct inode *ip) {
    int i, j;
    struct fsbuf *bp;
    uint *a;

    // 1. 释放直接块
    for (i = 0; i < NDIRECT; i++) {
        if (ip->addrs[i]) {
            fs_block_free(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    // 2. 释放间接块
    if (ip->addrs[NDIRECT]) {
        // 先读出间接块的内容，因为里面存着要释放的块号
        bp = fsbuf_read(ip->dev, ip->addrs[NDIRECT]);
        a = (uint *) bp->data;

        for (j = 0; j < NINDIRECT; j++) {
            if (a[j])
                fs_block_free(ip->dev, a[j]);
        }

        fsbuf_release(bp); // 释放间接块的缓存

        // 最后释放间接块本身
        fs_block_free(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    // 3. 更新 inode 元数据
    ip->size = 0;
    fs_inode_write(ip); // 写回磁盘
}

// 释放内存 inode 引用
// 如果这是最后一个引用，且 nlink 为 0，则彻底删除文件
void fs_inode_release(struct inode *ip) {
    fs_inode_lock(ip);
    if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
        // 触发彻底删除逻辑：
        // 释放所有数据块
        fs_inode_trunc(ip);
        // 标记 inode 为空闲 (type = 0)
        ip->type = 0;
        fs_inode_write(ip);
        ip->valid = 0; // 内存缓存也标记无效
    }

    ip->ref--;
    fs_inode_unlock(ip);
    // 如果 ref > 0，说明还有别人在用，我们只是减少引用
    // 如果 ref == 0，这个槽位现在空闲了，可以被 iget 复用
}

// 从 inode 读取数据到 dst
// is_user_addr: 是否是用户地址
// off: 文件偏移量
// n: 读取字节数
// 返回实际读取的字节数
int fs_inode_read_data(struct inode *ip, int is_user_addr, char *dst, uint off, uint n) {
    if (ip->valid == 0) {
        panic("fs_inode_read_data: invalid inode.");
    }
    uint tot, m;
    struct fsbuf *bp;

    // 1. 边界检查
    // 如果偏移量已经超过文件大小，读不到东西
    if (off > ip->size || off + n < off)
        return 0;
    // 如果读的长度超过文件剩余大小，截断
    if (off + n > ip->size)
        n = ip->size - off;

    // 2. 循环读取，以块为单位读取
    for (tot = 0; tot < n; tot += m, off += m, dst += m) {
        // bmap 找到物理块号
        uint addr = fs_inode_map(ip, off / BSIZE);
        // 计算本次能读多少：
        // 也就是：min(剩余总长度, 当前块剩余空间)
        m = min(n - tot, BSIZE - off % BSIZE);
        if (addr == 0) {
            // 稀疏区域：填充零
            if (is_user_addr) {
                // 用户空间：需要逐个字节或使用辅助函数清零
                char zero = 0;
                for (uint i = 0; i < m; i++) {
                    vmem_copyout(proc_running()->pagetable, (uint64) (dst + i), &zero, 1);
                }
            } else {
                // 内核空间：直接 memset
                memset(dst, 0, m);
            }
            continue; // 继续处理下一个块
        }

        bp = fsbuf_read(ip->dev, addr);
        // 拷贝数据
        if (is_user_addr)
            vmem_copyout(proc_running()->pagetable, (uint64) dst, (void *) bp->data + (off % BSIZE), m);
        else
            memmove(dst, bp->data + (off % BSIZE), m);

        fsbuf_release(bp);
    }
    return tot;
}

// 将 src 写入 inode
// off: 文件偏移量
// is_user_addr: 是否是用户地址
// n: 写入字节数
// 返回实际写入字节数
int fs_inode_write_data(struct inode *ip, int is_user_addr, char *src, uint off, uint n) {
    if (ip->valid == 0) {
        panic("fs_inode_write_data: invalid inode.");
    }
    uint tot, m;
    struct fsbuf *bp;

    // 1. 边界检查 (限制最大文件大小)
    if (off + n < off || (uint64) off + n > MAXFILE * BSIZE)
        return -1;

    // 2. 循环写入
    for (tot = 0; tot < n; tot += m, off += m, src += m) {
        // map 会自动分配不存在的块
        uint addr = fs_inode_map(ip, off / BSIZE);
        if (addr == 0)
            break; // 磁盘满或者是分配失败

        bp = fsbuf_read(ip->dev, addr);

        // 计算本次写入长度
        m = BSIZE - (off % BSIZE);
        if (n - tot < m)
            m = n - tot;

        // 拷贝数据
        if (is_user_addr)
            vmem_copyin(proc_running()->pagetable, (void *) bp->data + (off % BSIZE), (uint64) src, m);
        else
            memmove(bp->data + (off % BSIZE), src, m);

        // 标记脏并写回
        fslog_write(bp);
        fsbuf_release(bp);
    }

    // 3. 如果写入导致文件变大，更新 size
    if (n > 0 && off > ip->size) {
        ip->size = off;
        fs_inode_write(ip); // 更新 inode 元数据
    }

    return tot;
}

// ================ 目录相关 ================

// 在目录 dp 中查找名为 name 的文件
// 如果找到了，返回对应的 inode (已经 iget，引用计数+1)
// poff: 可选参数，如果不为0，则记录找到的目录项在目录文件内的偏移量
struct inode *fs_dir_lookup(struct inode *dp, char *name, uint *poff) {
    uint off, inum;
    struct dirent de;

    if (dp->type != T_DIR)
        panic("fs_dir_lookup: not a directory");

    // 遍历目录文件的内容
    // 每次读一个 dirent 大小
    for (off = 0; off < dp->size; off += sizeof(de)) {
        // 获取目录项内容
        if (fs_inode_read_data(dp, 0, (char *) &de, off, sizeof(de)) != sizeof(de))
            panic("fs_dir_lookup: read");
        // 如果 inum 为 0，说明这个槽位是空的，跳过
        if (de.inum == 0)
            continue;

        // 比较名字
        if (strcmp(name, de.name) == 0) {
            // 找到了
            if (poff)
                *poff = off;
            inum = de.inum;
            // 通过 inode 号获取内存 inode
            return fs_inode_get(dp->dev, inum);
        }
    }

    return 0; // 没找到
}

// 在目录 dp 中添加一个新的目录项 (name, inum)
int fs_dir_link(struct inode *dp, char *name, uint inum) {
    int off;
    struct dirent de;
    struct inode *ip;

    // 1. 先检查是否重名
    if ((ip = fs_dir_lookup(dp, name, 0)) != 0) {
        fs_inode_release(ip); // lookup 会增加引用，这里要释放
        return -1; // 已经存在了
    }

    if (strlen(name) > DIRSIZ) {
        // 文件名过大
        return -1;
    }

    // 2. 找一个空槽位 (inum == 0 的位置)
    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (fs_inode_read_data(dp, 0, (char *) &de, off, sizeof(de)) != sizeof(de))
            panic("fs_dir_link: read");
        if (de.inum == 0)
            break; // 找到了空位，就在这写
    }

    // 3. 准备目录项数据
    memset(&de, 0, sizeof(de)); // 🔥 先清零整个结构体！
    strncpy(de.name, name, DIRSIZ); // 使用 strncpy 更安全
    de.inum = inum;

    // 4. 写入目录文件 (如果是追加，write_data 会自动扩容)
    if (fs_inode_write_data(dp, 0, (char *) &de, off, sizeof(de)) != sizeof(de))
        panic("fs_dir_link: write");

    return 0;
}

// 解析路径，提取下一个文件名
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
static char *skipelem(char *path, char *name) {
    char *s;
    int len;

    while (*path == '/') path++;
    if (*path == 0) return 0;

    s = path;
    while (*path != '/' && *path != 0) path++;
    len = path - s;

    if (len >= DIRSIZ)
        memmove(name, s, DIRSIZ);
    else {
        memmove(name, s, len);
        memset(name + len, 0, DIRSIZ - len); // 把剩下的全清零，最稳妥
    }

    while (*path == '/') path++;
    return path;
}

// 解析路径 path，返回对应的 inode
// 如果 nameiparent 为真，返回最后一个元素的父目录 (用于创建文件时)
// name: 用于传出最后一个元素的名称
// 为什么需要“父目录”版本？
// 因为很多操作都需要 “父目录 + 最后一个名字”，比如:
// 创建文件：
// 需要 父目录 inode，在里面插入一条 name → inum（dirlink）
// 再分配一个新的 inode（ialloc）做内容
// unlink 删除文件：
// 需要 父目录 inode，从里面删掉该名字（把 dirent.inum 置 0）
// 再 iput 目标 inode（可能触发 itrunc）
// 找到的 inode 不是最新的！需要读盘！
static struct inode *namex(char *path, int nameiparent, char *name) {
    struct inode *ip, *next;

    // 1. 确定起点：是绝对路径(/) 还是 相对路径?
    if (*path == '/')
        ip = fs_inode_get(ROOTDEV, ROOT_INODE); // 从根目录开始
    else {
        ip = proc_running()->cwd;
        ip->ref++;
    }

    while ((path = skipelem(path, name)) != 0) {
        // 锁定当前目录，准备读取
        fs_inode_lock(ip);
        fs_inode_read(ip);

        if (ip->type != T_DIR) {
            fs_inode_unlock(ip);
            fs_inode_release(ip);
            return 0;
        }

        // 如果是要找父目录，且这是最后一级，直接返回当前目录
        if (nameiparent && *path == '\0') {
            // 不要 put ip，因为我们要返回它
            fs_inode_unlock(ip);
            return ip;
        }

        // 查找下一级
        if ((next = fs_dir_lookup(ip, name, 0)) == 0) {
            fs_inode_unlock(ip);
            fs_inode_release(ip); // 没找到
            return 0;
        }

        // 继续下一层
        fs_inode_unlock(ip);
        fs_inode_release(ip); // 释放当前层
        ip = next;
    }

    if (nameiparent) {
        fs_inode_release(ip);
        return 0;
    }
    return ip;
}

// 接口 1: 返回路径对应的 inode
struct inode *fs_namei(char *path) {
    char name[DIRSIZ];
    return namex(path, 0, name);
}

// 接口 2: 返回路径父目录的 inode (用于创建)
struct inode *fs_nameiparent(char *path, char *name) {
    return namex(path, 1, name);
}

// ================ 信息显示 =================
// 统计空闲数据块数量
uint64 fs_count_free_blocks(int dev) {
    uint64 free_count = 0;
    struct fsbuf *bp;
    int bi, m;

    // 遍历所有 bitmap 块
    for (uint b = 0; b < sb.size; b += BPB) {
        bp = fsbuf_read(dev, BBLOCK(b, sb));
        for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
            m = 1 << (bi % 8);
            if (((bp->data[bi / 8] & m) == 0)) {
                // 0 表示空闲
                free_count++;
            }
        }
        fsbuf_release(bp);
    }
    return free_count;
}

// 统计空闲 Inode 数量
uint64 fs_count_free_inodes(int dev) {
    uint64 free_count = 0;
    struct fsbuf *bp;
    struct dinode *dip;

    for (int inum = 1; inum < sb.ninodes; inum++) {
        bp = fsbuf_read(dev, IBLOCK(inum, sb));
        dip = (struct dinode *) bp->data + (inum % IPB);
        if (dip->type == 0) {
            free_count++;
        }
        fsbuf_release(bp);
    }
    return free_count;
}

// 获取文件系统信息接口 (供系统调用使用)
// 我们可以定义一个 struct fs_info
void fs_get_info(int dev, uint64 *total_blocks, uint64 *free_blocks, uint64 *total_inodes, uint64 *free_inodes) {
    *total_blocks = sb.nblocks; // 或者 sb.size，看你想显示哪个
    *free_blocks = fs_count_free_blocks(dev);
    *total_inodes = sb.ninodes;
    *free_inodes = fs_count_free_inodes(dev);
}


// ================ 测试相关 =================
// 打印二进制位
void print_byte_binary(uchar b) {
    for (int i = 0; i < 8; i++) {
        printf("%d", (b >> i) & 1);
    }
}

// 验证 Bitmap 一致性与边界安全性
void fs_test_bitmap(int dev) {
    printf("--- [TEST] Checking Bitmap Consistency ---\n");

    uint nbitmap = (sb.size + BPB - 1) / BPB;
    uint datastart = sb.bmapstart + nbitmap;

    printf("  > Superblock info: size=%d, bmapstart=%d\n", sb.size, sb.bmapstart);
    printf("  > Calculated datastart: block %d\n", datastart);

    int err = 0;

    // 不仅检查 size 范围内的，还要检查整个 Bitmap 块能覆盖的范围
    // 假设只用了1个bitmap块，就要检查到 8191
    uint check_limit = nbitmap * BPB;

    printf("  > Checking bits 0 to %d (Full Bitmap Range)...\n", check_limit - 1);

    for (uint b = 0; b < check_limit; b++) {
        struct fsbuf *bp = fsbuf_read(dev, BBLOCK(b, sb));
        int bi = b % BPB;
        int byte_idx = bi / 8;
        int bit_idx = bi % 8;
        int is_used = (bp->data[byte_idx] >> bit_idx) & 1;
        fsbuf_release(bp);

        if (b < datastart) {
            // [0 ~ datastart-1]: 元数据区 -> 必须是 1 (Used)
            if (is_used == 0) {
                printf("❌ Error: Block %d (Metadata) is FREE!\n", b);
                err++;
            }
        } else if (b < sb.size) {
            // [datastart ~ size-1]: 有效数据区 -> 必须是 0 (Free)
            if (is_used == 1) {
                printf("❌ Error: Block %d (Data) is USED!\n", b);
                err++;
            }
        } else {
            // [size ~ limit]: 越界区域 -> 必须是 1 (Used/Guard)
            if (is_used == 0) {
                printf("❌ Error: Block %d (Out of Bound) is FREE! balloc might allocate it!\n", b);
                err++;
            }
        }

        if (err > 5) {
            printf_color("...too many errors, stopping test.\n",RED);
            break;
        }
    }

    if (err == 0) {
        printf("✅ [SUCCESS] Bitmap layout perfect:\n");
        printf("    [0...%d] Metadata (USED)\n", datastart - 1);
        printf("    [%d...%d] Data Space (FREE)\n", datastart, sb.size - 1);
        printf("    [%d...%d] Out of Bound (USED/Guarded)\n", sb.size, check_limit - 1);
    } else {
        printf_color("Bitmap check failed\n",RED);
    }
}

// 测试磁盘数据块分配和释放
void fs_test_alloc(int dev) {
    // 测试是否正常分配和释放磁盘块，fs_test_bitmap报错是正常的
    printf("--- [TEST] Block Alloc/Free ---\n");

    // 1. 尝试分配一个块
    uint b1 = fs_block_alloc(dev);
    printf("Allocated block: %d\n", b1);

    // 验证：必须在数据区
    if (b1 < 47 || b1 >= sb.size) panic("Allocated metadata/invalid block!");

    // 验证：必须是清零的
    struct fsbuf *bp = fsbuf_read(dev, b1);
    for (int i = 0; i < BSIZE; i++) {
        if (bp->data[i] != 0) panic("Block not zeroed!");
    }
    fsbuf_release(bp);

    // 2. 再分配一个
    uint b2 = fs_block_alloc(dev);
    printf("Allocated block: %d\n", b2);
    if (b2 != b1 + 1) printf("⚠️ [INFO] blocks not sequential (this is fine)\n");

    // 验证：bitmap
    fs_test_bitmap(dev);

    // 3. 释放第一个块
    printf("Freeing block %d...\n", b1);
    fs_block_free(dev, b1);

    // 验证：bitmap
    fs_test_bitmap(dev);

    // 4. 再次分配，应该优先拿到刚才释放的 b1 (因为它是第一个空闲位)
    uint b3 = fs_block_alloc(dev);
    printf("Allocated block: %d\n", b3);

    if (b3 == b1) {
        printf("✅ [SUCCESS] Block Alloc/Free works (Recycled correctly)!\n");
    } else {
        // 也不一定非要相等
        printf("⚠️ [INFO] Allocator did not recycle immediately, got %d\n", b3);
    }

    // 释放块保证原子性
    fs_block_free(dev, b2);
    fs_block_free(dev, b3);
    fs_test_bitmap(dev);
}


// 测试 inode 层
void fs_test_inode(int dev) {
    printf("--- [TEST] Inode Layer ---\n");
    fs_test_bitmap(dev);

    // 1. 分配一个新 inode (模拟新建文件)
    // T_FILE = 2
    struct inode *ip = fs_inode_alloc(dev, T_FILE);
    printf("Allocated Inode: %d, type: %d\n", ip->inum, ip->type);

    // 刚分配的 inode，ref 应该是 1
    if (ip->ref != 1) panic("ref cnt error");

    // 2. 模拟写入数据：映射第 0 块和第 200 块
    // 这会自动触发 fs_block_alloc
    uint b0 = fs_inode_map(ip, 0);
    printf("Mapped logical block 0 -> physical %d\n", b0);

    uint b200 = fs_inode_map(ip, 200); // 触发间接块分配
    printf("Mapped logical block 200 -> physical %d\n", b200);

    // 这里检查会找到3个被用过的block
    fs_test_bitmap(dev);

    // 3. 模拟删除文件
    // 设置 nlink = 0 (表示没有目录指向它了)
    ip->nlink = 0;
    // 释放引用 (因为 nlink=0 且 ref=1，这会触发 trunc 和 free)
    printf("Releasing inode (triggering deletion)...\n");
    fs_inode_release(ip);

    // 4. 验证释放是否成功
    // 再次读取这个 inode，type 应该是 0 (空闲)
    //手动去读磁盘，绕过 iget 的缓存命中逻辑来验证磁盘状态
    struct fsbuf *bp = fsbuf_read(dev, IBLOCK(ip->inum, sb));
    struct dinode *dip = (struct dinode *) bp->data + (ip->inum % IPB);

    if (dip->type == 0) {
        printf("✅ [SUCCESS] Inode deleted and marked free on disk.\n");
    } else {
        printf("❌ [FAIL] Inode type is %d (expected 0)\n", dip->type);
        panic("Inode delete failed");
    }
    fsbuf_release(bp);
    // 这里检查会全部空闲
    fs_test_bitmap(dev);
}

// 测试读写inode具体数据
void fs_test_rw(int dev) {
    printf("--- [TEST] File Read/Write ---\n");

    // 1. 创建文件
    struct inode *ip = fs_inode_alloc(dev, T_FILE);
    printf("Created inode %d\n", ip->inum);

    // 2. 写入数据 (跨块测试)
    // 写入 "Hello World" 到偏移 0
    char *msg1 = "Hello World!";
    int n1 = fs_inode_write_data(ip, 0, msg1, 0, strlen(msg1) + 1);
    printf("Wrote %d bytes at offset 0\n", n1);

    // 写入长数据到偏移 1000 (跨越 1024 边界)
    // 这会在 0~1000 之间产生空洞 (Sparse hole)，bmap 会分配中间的块吗？
    // 注意：writei 是按字节循环的，如果从 1000 开始写，中间的 12~999 还是空的。
    // 但 fs_inode_map 是按块分配的。如果写 1000，第 0 块还在。

    char big_buf[200];
    memset(big_buf, 'A', sizeof(big_buf));
    // 从 1000 写到 1200 (跨越 Block 0 和 Block 1)
    int n2 = fs_inode_write_data(ip, 0, big_buf, 1000, 200);
    printf("Wrote %d bytes at offset 1000 (Cross block boundary)\n", n2);

    // 此时文件大小应该是 1200
    if (ip->size != 1200) panic("File size incorrect");

    // 3. 读取并验证
    char read_buf[300];

    // 读开头
    fs_inode_read_data(ip, 0, read_buf, 0, 13);
    if (strcmp(read_buf, "Hello World!") != 0) panic("Read content mismatch at 0");
    printf("Read offset 0: %s ✅ \n", read_buf);

    // 读跨界处 (Block 0 end -> Block 1 start)
    // 偏移 1023 是 Block 0 的最后一个字节
    // 偏移 1024 是 Block 1 的第一个字节
    char cross_buf[4];
    fs_inode_read_data(ip, 0, cross_buf, 1023, 2);
    if (cross_buf[0] == 'A' && cross_buf[1] == 'A') {
        printf("Read cross-block boundary: ✅ \n");
    } else {
        panic("Read cross-block failed");
    }
    fs_test_bitmap(1);
    // 4. 稀疏文件测试
    fs_inode_write_data(ip, 0, big_buf, 8 * BSIZE, 200);
    printf("After write offset: 8192\n");
    fs_test_bitmap(1);
    if (ip->size != 8192 + 200) panic("File size incorrect");
    fs_inode_read_data(ip, 0, read_buf, 8 * BSIZE - 50, 300);

    // 5. 清理
    ip->nlink = 0;
    fs_inode_release(ip);
    printf("✅ [SUCCESS] Read/Write test passed!\n");
}

// 目录解析测试
void fs_test_dir(int dev) {
    printf("--- [TEST] Directory Layer ---\n");

    // 1. 需要手动初始化根目录 (因为 mkfs 没做)
    printf("Initializing ROOT directory...\n");
    // 正常 OS 启动时，mount 过程会保证根目录存在
    struct inode *root = fs_inode_get(dev, ROOT_INODE);
    fs_inode_read(root);

    // 2. 在根目录下创建一个新文件 "hello"
    printf("Creating /hello ...\n");
    struct inode *f1 = fs_inode_alloc(dev, T_FILE);
    // 链接到根目录
    if (fs_dir_link(root, "hello", f1->inum) < 0)
        panic("link error");
    f1->nlink++;
    fs_inode_write(f1);

    fs_test_bitmap(1);

    printf("Created file 'hello' with inum %d linked to root\n", f1->inum);
    fs_inode_release(f1); // 释放 f1，只留目录里的链接

    // 3. 通过路径查找 (/hello)
    printf("Looking up /hello ...\n");
    struct inode *f2 = fs_namei("/hello");
    if (f2 == 0) {
        panic("namei failed");
        return;
    }

    printf("Found /hello! inum=%d\n", f2->inum);

    if (f2->type != T_FILE) panic("Type error");

    f2->nlink--;
    root->nlink--;
    fs_inode_release(f2);
    fs_inode_release(root);

    printf("✅ [SUCCESS] Directory test passed!\n");
    fs_test_bitmap(1);
}

// 递归目录解析测试
void fs_test_recursive(int dev) {
    printf("--- [TEST] Recursive Directory & Path Parsing ---\n");

    struct inode *root = fs_inode_get(dev, ROOT_INODE);
    fs_inode_read(root);

    // 1. 创建第一级目录: /level1
    printf("Creating /level1 ...\n");
    struct inode *d1 = fs_inode_alloc(dev, T_DIR);
    // 注意：标准的 mkdir 还需要在 d1 里写入 "." 和 ".."
    // 这里为了简化测试 namex 逻辑，我们只做父目录到子目录的链接
    if (fs_dir_link(root, "level1", d1->inum) < 0)
        panic("link level1 failed");
    d1->nlink++;
    fs_inode_write(d1);
    // 释放 d1，强迫我们等会儿必须通过查找来获取它
    fs_inode_release(d1);

    // 2. 创建第二级目录: /level1/level2
    printf("Creating /level1/level2 ...\n");
    // 先找到父目录 /level1
    struct inode *parent = fs_namei("/level1");
    if (parent == 0) panic("failed to find /level1");
    if (parent->type != T_DIR) panic("/level1 is not a dir");

    struct inode *d2 = fs_inode_alloc(dev, T_DIR);
    fs_inode_read(parent);
    if (fs_dir_link(parent, "level2", d2->inum) < 0)
        panic("link level2 failed");
    d2->nlink++;
    fs_inode_write(d2);
    fs_inode_release(d2);
    fs_inode_release(parent); // 释放 /level1

    // 3. 在深层创建文件: /level1/level2/deep.txt
    printf("Creating /level1/level2/deep.txt ...\n");
    // 这里的路径解析会经过 root -> level1 -> level2
    parent = fs_namei("/level1/level2");
    if (parent == 0) panic("failed to find /level1/level2");

    struct inode *f = fs_inode_alloc(dev, T_FILE);
    fs_inode_read(parent);
    if (fs_dir_link(parent, "deep.txt", f->inum) < 0)
        panic("link deep.txt failed");
    f->nlink++;
    fs_inode_write(f);
    // 写入一些秘密数据
    char *secret = "The answer is 42";
    fs_inode_write_data(f, 0, secret, 0, strlen(secret) + 1);

    fs_inode_release(f);
    fs_inode_release(parent); // 释放 /level1/level2

    // 4. 终极验证：一次性解析全路径读取
    printf("Verifying full path lookup ...\n");
    struct inode *target = fs_namei("/level1/level2/deep.txt");
    fs_inode_read(target);
    if (target == 0) panic("fs_namei failed for deep path");

    char buf[32];
    fs_inode_read_data(target, 0, buf, 0, sizeof(buf));
    printf("Read from deep path: %s\n", buf);

    if (strcmp(buf, secret) != 0) panic("Content mismatch");

    fs_inode_release(target);
    fs_inode_release(root);

    printf("✅ [SUCCESS] Recursive Directory test passed!\n");
}

// 简单的整数转字符串辅助函数
static void itoa_simple(int n, char *s) {
    int i = 0, j;
    char temp[16];
    if (n == 0) {
        s[0] = '0';
        s[1] = '\0';
        return;
    }
    while (n > 0) {
        temp[i++] = (n % 10) + '0';
        n /= 10;
    }
    for (j = 0; j < i; j++) s[j] = temp[i - 1 - j];
    s[i] = '\0';
}

// 压力测试
void fs_test_stress(int dev) {
    printf("\n💀 [STRESS TEST] Starting Hell Mode...\n");

    struct inode *root = fs_inode_get(dev, ROOT_INODE);
    fs_inode_read(root);
    // root->type = T_DIR;
    // root->nlink = 1;
    // fs_inode_write(root);

    // =============================================
    // 关卡 1: 大文件测试 (测试间接块)
    // =============================================
    printf("\n🌊 [Level 1] Indirect Block (Large File)...\n");

    struct inode *huge = fs_inode_alloc(dev, T_FILE);
    // 链接到根目录以便后续查找
    if (fs_dir_link(root, "huge_file", huge->inum) < 0) panic("link huge failed");
    huge->nlink++;
    fs_inode_write(huge);

    // 写入 14KB 数据 (直接块只有 12KB，这里必然触发间接块)
    // 我们写入一个特殊的 pattern，比如每个字节都是对应偏移量的低8位
    char buf[BSIZE];
    int total_size = 14 * 1024;

    printf("   Writing %d bytes...\n", total_size);
    for (int i = 0; i < total_size; i += BSIZE) {
        // 构造数据 pattern
        for (int j = 0; j < BSIZE; j++) buf[j] = (char) ((i + j) & 0xFF);

        int n = fs_inode_write_data(huge, 0, buf, i, BSIZE);
        if (n != BSIZE) panic("huge write failed");
    }

    printf("   Verifying data consistency...\n");
    // 重新读取验证
    for (int i = 0; i < total_size; i += BSIZE) {
        memset(buf, 0, BSIZE);
        fs_inode_read_data(huge, 0, buf, i, BSIZE);
        for (int j = 0; j < BSIZE; j++) {
            if (buf[j] != (char) ((i + j) & 0xFF)) {
                printf("Error at offset %d: expected %x got %x\n", i + j, (i + j) & 0xFF, buf[j]);
                panic("Data mismatch in huge file");
            }
        }
    }
    printf("   ✅ Large file read/write OK (Indirect blocks work!)\n");

    // 释放 huge，强制写回
    fs_inode_release(huge);

    // =============================================
    // 关卡 2: 目录爆炸测试 (测试目录扩容)
    // =============================================
    printf("\n💣 [Level 2] Directory Expansion (Many Files)...\n");

    struct inode *dir = fs_inode_alloc(dev, T_DIR);
    if (fs_dir_link(root, "many_files", dir->inum) < 0) panic("link dir failed");
    dir->nlink++; // 指向自己
    dir->nlink++; // 根目录指向它
    fs_inode_write(dir);

    // 一个块 1024 字节，dirent 16 字节，一个块能存 64 个。
    // 我们创建 70 个文件，强制它分配第二个块。
    int file_count = 70;
    printf("   Creating %d files in /many_files/ ...\n", file_count);

    for (int i = 0; i < file_count; i++) {
        struct inode *f = fs_inode_alloc(dev, T_FILE);
        char name[DIRSIZ];
        char num[8];
        itoa_simple(i, num);

        // 名字是 f0, f1, ... f69
        memset(name, 0, DIRSIZ);
        name[0] = 'f';
        strcpy(name + 1, num);

        if (fs_dir_link(dir, name, f->inum) < 0) panic("link failed in loop");

        f->nlink++;
        fs_inode_write(f);
        fs_inode_release(f);
    }

    printf("   Directory size is now: %d bytes (Expected > 1024)\n", dir->size);
    if (dir->size <= 1024) panic("Directory did not expand to 2nd block!");

    fs_inode_release(dir); // 释放目录

    // =============================================
    // 关卡 3: 持久化查找验证
    // =============================================
    printf("\n🕵️ [Level 3] Persistence Lookup Verification...\n");

    // 1. 找回大文件
    struct inode *target_huge = fs_namei("/huge_file");
    if (target_huge == 0) panic("Lost /huge_file");
    fs_inode_read(target_huge); // 🔥 记得读盘！
    if (target_huge->size != total_size) panic("Huge file size incorrect");
    printf("   Found /huge_file (size %d) ✅ \n", target_huge->size);
    fs_inode_release(target_huge);

    // 2. 找回第 68 号小文件
    struct inode *target_small = fs_namei("/many_files/f68");
    if (target_small == 0) panic("Lost /many_files/f68");
    fs_inode_read(target_small); // 🔥 记得读盘！
    printf("   Found /many_files/f68 (inum %d) ✅ \n", target_small->inum);
    fs_inode_release(target_small);

    fs_inode_release(root);
    printf("\n🎉🎉 [VICTORY] ALL HELL MODE TESTS PASSED! 🎉🎉\n");
}
