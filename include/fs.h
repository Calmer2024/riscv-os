#ifndef RISCV_OS_FS_H
#define RISCV_OS_FS_H
#include "param.h"
#include "sleeplock.h"
#include "types.h"

#define ROOTDEV 1        // 根磁盘设备号
#define ROOT_INODE 1      // 根目录的 Inode 编号通常是 1
#define BSIZE 1024  // 块大小
#define FSMAGIC 0x88888888 // 文件系统魔数

// [ boot | super | log | inode blocks | bitmap | data blocks ]

// 磁盘上一块在内存中对应的缓存
struct fsbuf {
    int valid; // has data been read from disk?
    int disk; // does disk "own" buf?
    uint dev;
    uint blockno;
    uint refcnt;
    struct fsbuf *prev; // LRU cache list
    struct fsbuf *next;
    uchar data[BSIZE];
    struct sleeplock lock; // 保护这个 buffer 的内容
};

// 超级块结构
struct superblock {
    uint magic; // 魔数，验证文件系统是否合法
    uint size; // 文件系统总块数
    uint nblocks; // 数据块数量
    uint ninodes; // Inode 数量
    uint nlog; // 日志块数量
    uint logstart; // 日志区起始块号
    uint inodestart; // Inode 区起始块号
    uint bmapstart; // Bitmap 起始块号
};

// 磁盘上的日志头结构
struct fslog_header {
    uint n; // 当前日志里有多少个有效块
    uint block_nums[LOGBLOCKS]; // 记录每个日志块原本属于磁盘哪个位置
};

// Inode 里的直接块数量
#define NDIRECT 12
// 一个间接块能存多少个指针？(1024 / 4 = 256)
#define NINDIRECT (BSIZE / sizeof(uint))
// 一个文件的最大块数
#define MAXFILE (NDIRECT + NINDIRECT)

// 文件类型
#define T_DIR     1   // 目录
#define T_FILE    2   // 普通文件
#define T_DEVICE  3   // 设备文件

// 磁盘上的inode结构
struct dinode {
    short type; // 文件类型 (0代表空闲)
    short major; // 主设备号 (T_DEVICE only)
    short minor; // 次设备号 (T_DEVICE only)
    short nlink; // 硬链接计数
    uint size; // 文件大小(字节)
    uint addrs[NDIRECT + 1]; // 数据块地址 (12个直接 + 1个间接)
};

// 内存中的 Inode (In-memory copy of an inode)
struct inode {
    uint dev; // 设备号
    uint inum; // Inode 编号
    int ref; // 引用计数 (有多少个指针指向它?)
    int valid; // 数据是否已从磁盘读入? (类似 fsbuf.valid)
    // 复制自 dinode
    short type;
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
    struct sleeplock lock;
};


// 文件名大小
#define DIRSIZ 14

// 目录项每一项结构
struct dirent {
    ushort inum; // Inode 编号
    char name[DIRSIZ]; // 文件名
};

// ================= 核心定位宏定义 =================

// 计算一个块能存多少个 dinode
#define IPB (BSIZE / sizeof(struct dinode))

// 计算第 i 号 inode 位于磁盘的第几块
// 公式：inode区起始块 + (i / 每块数量)
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// 计算一个块能存多少个 bitmap 位 (1024 * 8 = 8192)
#define BPB (BSIZE*8)

// 计算第 b 个数据块对应的 bitmap 位于磁盘的第几块
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)

// virtio_disk.c
void virtio_disk_init(void);

void virtio_disk_rw(struct fsbuf *b, int write);

void virtio_disk_intr();

void virtio_disk_test(void);

// fsbuf.c
void fsbuf_init(void);

struct fsbuf *fsbuf_read(uint dev, uint blockno);

void fsbuf_write(struct fsbuf *b);

void fsbuf_release(struct fsbuf *b);

void fsbuf_dump_list(void);

void fsbuf_test(void);

void fsbuf_test_lru(void);

// fs.c
void fs_init(int dev, int debug);

void fs_inode_lock(struct inode *ip);

void fs_inode_unlock(struct inode *ip);

struct inode *fs_inode_get(uint dev, uint inum);

void fs_inode_read(struct inode *ip);

void fs_inode_write(struct inode *ip);

uint fs_inode_map(struct inode *ip, uint bn);

struct inode *fs_inode_alloc(uint dev, short type);

void fs_inode_trunc(struct inode *ip);

void fs_inode_release(struct inode *ip);

int fs_inode_read_data(struct inode *ip, int is_user_addr, char *dst, uint off, uint n);

int fs_inode_write_data(struct inode *ip, int is_user_addr, char *src, uint off, uint n);

struct inode *fs_dir_lookup(struct inode *dp, char *name, uint *poff);

int fs_dir_link(struct inode *dp, char *name, uint inum);

struct inode *fs_namei(char *path);

struct inode *fs_nameiparent(char *path, char *name);

void fs_get_info(int dev, uint64 *total_blocks, uint64 *free_blocks, uint64 *total_inodes, uint64 *free_inodes);

void fs_test_bitmap(int dev);

void fs_test_alloc(int dev);

void fs_test_inode(int dev);

void fs_test_rw(int dev);

void fs_test_dir(int dev);

void fs_test_recursive(int dev);

void fs_test_stress(int dev);

// fslog.c
void fslog_init(int dev, struct superblock *sb, int debug);

void fslog_write(struct fsbuf *b);

void fslog_op_begin();

void fslog_op_end();

#endif //RISCV_OS_FS_H
