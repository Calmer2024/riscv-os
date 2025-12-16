#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "include/fs.h"
#include "include/param.h"


// ==========================================

// 创建的 inode 块数量
#define NINODES 200

// 全局变量，方便各函数访问
int fsfd;
struct superblock sb;
int datastart_block; // 数据区起始块号

int next_free_block; // 指向下一个可用的空闲数据块

// ==========================================
// 1. 基础 IO 辅助函数
// ==========================================

void die(const char *msg) {
    perror(msg);
    exit(1);
}

void die_fmt(const char *fmt, const char *arg) {
    fprintf(stderr, fmt, arg);
    fprintf(stderr, "\n");
    exit(1);
}

// 辅助：转换 inode 号到磁盘偏移量
unsigned int inode_offset(unsigned int i) {
    return (sb.inodestart * BSIZE) + (i * sizeof(struct dinode));
}

// 写入一个磁盘块
void write_block(int blockno, void *data) {
    if (lseek(fsfd, blockno * BSIZE, SEEK_SET) != blockno * BSIZE) {
        die("write_block: lseek failed");
    }
    if (write(fsfd, data, BSIZE) != BSIZE) {
        die("write_block: write failed");
    }
}

// 读取一个磁盘块 (用于调试或回读)
void read_block(int blockno, void *data) {
    if (lseek(fsfd, blockno * BSIZE, SEEK_SET) != blockno * BSIZE) {
        die("read_block: lseek failed");
    }
    if (read(fsfd, data, BSIZE) != BSIZE) {
        die("read_block: read failed");
    }
}

// 写入一个 Inode
void write_inode(uint inum, struct dinode *ip) {
    off_t off = inode_offset(inum);
    if (lseek(fsfd, off, SEEK_SET) != off) {
        die("write_inode: lseek failed");
    }
    if (write(fsfd, ip, sizeof(*ip)) != sizeof(*ip)) {
        die("write_inode: write failed");
    }
}

// 分配一个空闲数据块，返回块号
int alloc_block() {
    if (next_free_block >= FSSIZE) {
        die("mkfs: out of blocks (fs.img too small?)");
    }
    // 返回当前可用块，并将水位线推高
    // 注意：这里我们只分配块号，实际写入由调用者负责
    // 调用者必须保证写入该块，否则该块内容可能是脏的
    return next_free_block++;
}

// ==========================================
// 2. 文件系统初始化逻辑
// ==========================================

// 初始化超级块参数
void init_superblock() {
    int nbitmap = (FSSIZE + BPB - 1) / BPB;
    int ninodeblocks = (NINODES + IPB - 1) / IPB;
    int nlog = LOGBLOCKS + 1;

    // 计算元数据区大小
    int nmeta = 2 + nlog + ninodeblocks + nbitmap;

    // 填充全局 sb 结构体
    sb.magic = FSMAGIC;
    sb.size = FSSIZE;
    sb.nblocks = FSSIZE - nmeta; // 数据块总数
    sb.ninodes = NINODES;
    sb.nlog = nlog;
    sb.logstart = 2;
    sb.inodestart = 2 + nlog;
    sb.bmapstart = 2 + nlog + ninodeblocks;

    // 计算数据区起始位置（供后续使用）
    datastart_block = nmeta;
    next_free_block = datastart_block + 1;


    printf("Layout: Meta=%d blocks, DataStart=%d, DataBlocks=%d\n",
           nmeta, datastart_block, sb.nblocks);
}

// 将整个磁盘清零
void zero_disk() {
    char buf[BSIZE];
    memset(buf, 0, BSIZE);
    for (int i = 0; i < FSSIZE; i++) {
        write_block(i, buf);
    }
}

// 写入超级块到第1块
void write_superblock() {
    // 超级块通常包含一些 padding，直接用 write_block 可能会覆盖整个块
    // 这里为了安全，先定位再写结构体大小，或者使用 buffer
    char buf[BSIZE];
    memset(buf, 0, BSIZE);
    memmove(buf, &sb, sizeof(sb)); // 拷贝到 buffer 头部
    write_block(1, buf);
}

// 初始化位图：标记元数据区为占用(1)，数据区为空闲(0)
void init_bitmap() {
    int total_bits = FSSIZE;
    int bitmap_bytes = (total_bits + 7) / 8;
    int nbitmap_blocks = (FSSIZE + BPB - 1) / BPB;

    char *bitmap = malloc(bitmap_bytes);
    if (!bitmap) {
        perror("malloc bitmap");
        exit(1);
    }

    // 1. 全部标记为占用 (1)
    memset(bitmap, 0xFF, bitmap_bytes);

    // 2. 将有效空闲区标记为 0 (Free)
    for (int i = next_free_block; i < FSSIZE; i++) {
        int byte = i / 8;
        int bit = i % 8;
        bitmap[byte] &= ~(1 << bit); // 清零该位
    }

    // 3. 写入磁盘 (处理跨块和填充)
    char buf[BSIZE];
    int remaining = bitmap_bytes;
    char *ptr = bitmap;

    for (int i = 0; i < nbitmap_blocks; i++) {
        // 默认填充 0xFF (防止最后一块只有一半数据时，尾部变成 00)
        memset(buf, 0xff, BSIZE);

        int copy_size = (remaining > BSIZE) ? BSIZE : remaining;
        memmove(buf, ptr, copy_size);

        write_block(sb.bmapstart + i, buf);

        ptr += copy_size;
        remaining -= copy_size;
    }

    free(bitmap);
    printf("Bitmap updated. Used blocks: 0-%d. Free blocks: %d-%d.\n",
           next_free_block - 1, next_free_block, FSSIZE - 1);
}

// ==========================================
// 3. 目录与文件创建逻辑
// ==========================================

// 初始化根目录 (Inode 1)
void init_root_dir() {
    struct dirent de[2];

    // 创建 "."
    memset(&de[0], 0, sizeof(struct dirent));
    de[0].inum = ROOT_INODE;
    strcpy(de[0].name, ".");

    // 创建 ".."
    memset(&de[1], 0, sizeof(struct dirent));
    de[1].inum = ROOT_INODE;
    strcpy(de[1].name, "..");

    // 1. 写入目录内容到数据块
    // 这里的 datastart_block 就是根目录的数据块
    // 注意：write_block 写入的是整个块，这里我们构造一个 buffer
    char buf[BSIZE];
    memset(buf, 0, BSIZE);
    memmove(buf, de, sizeof(de));
    write_block(datastart_block, buf);

    // 2. 创建并写入 Inode
    struct dinode din;
    memset(&din, 0, sizeof(din));
    din.type = T_DIR;
    din.major = 0;
    din.minor = 0;
    din.nlink = 1;
    din.size = sizeof(de);
    din.addrs[0] = datastart_block; // 映射到刚刚写的数据块

    write_inode(ROOT_INODE, &din);
}

// 添加 Console 设备文件 (Inode 2)
void add_console_device() {
    unsigned int console_inum = ROOT_INODE + 1; // 2

    // 1. 构造目录项
    struct dirent de;
    memset(&de, 0, sizeof(de));
    de.inum = console_inum;
    strncpy(de.name, "console", DIRSIZ);

    // 2. 将目录项追加到根目录数据块
    // 我们知道根目录数据块在 datastart_block
    // 且前 32 字节已被 . 和 .. 占用
    // 更好的做法是读取当前 Inode size，但在 mkfs 阶段我们知道布局，直接算偏移
    off_t offset = datastart_block * BSIZE + 2 * sizeof(struct dirent);

    if (lseek(fsfd, offset, SEEK_SET) != offset) {
        perror("seek root dir for console");
        exit(1);
    }
    if (write(fsfd, &de, sizeof(de)) != sizeof(de)) {
        perror("write console dirent");
        exit(1);
    }

    // 3. 更新根目录 Inode 的 size
    // 为了简单，我们这里重新读取 Inode 或者直接构造新的覆盖
    // 这里选择读取-修改-写入
    struct dinode root_din;
    off_t root_off = inode_offset(ROOT_INODE);
    lseek(fsfd, root_off, SEEK_SET);
    read(fsfd, &root_din, sizeof(root_din));

    root_din.size += sizeof(struct dirent);
    write_inode(ROOT_INODE, &root_din);

    // 4. 创建 Console Inode
    struct dinode din_console;
    memset(&din_console, 0, sizeof(din_console));
    din_console.type = T_DEVICE;
    din_console.major = 1;
    din_console.minor = 1;
    din_console.nlink = 1;
    din_console.size = 0;

    write_inode(console_inum, &din_console);
}

// 将宿主机文件写入镜像
// host_path: 宿主机上的文件路径 (如 "user/init")
// fs_name:   文件系统里的文件名 (如 "init")
// 核心：添加用户程序 (支持间接块！)
void append_user_program(char *host_path, char *fs_name) {
    int fd = open(host_path, O_RDONLY);
    if (fd < 0) {
        die_fmt("Cannot open host file: %s", host_path);
    }

    // 1. 分配 Inode
    static int next_inode_num = 3;
    int inum = next_inode_num++;
    if (inum >= NINODES) die("mkfs: too many files");

    struct dinode din;
    memset(&din, 0, sizeof(din));
    din.type = T_FILE;
    din.nlink = 1;
    din.size = 0;

    // 准备间接块缓冲区 (如果需要)
    // 我们先把间接表存在内存里，文件写完后再写入磁盘
    uint indirect[NINDIRECT];
    memset(indirect, 0, sizeof(indirect));
    int indirect_allocated_block = 0; // 记录间接表占用的磁盘块号

    char buf[BSIZE];
    int n;

    // 2. 读取并写入数据
    while ((n = read(fd, buf, BSIZE)) > 0) {
        int logic_block_idx = din.size / BSIZE;

        if (logic_block_idx >= MAXFILE) {
            die_fmt("File too large: %s", fs_name);
        }

        // 分配数据块
        int b = alloc_block();

        // 填充不足一个块的部分 (防止脏数据泄露)
        if (n < BSIZE) {
            memset(buf + n, 0, BSIZE - n);
        }
        write_block(b, buf);

        // 建立映射
        if (logic_block_idx < NDIRECT) {
            // 直接块
            din.addrs[logic_block_idx] = b;
        } else {
            // 间接块
            // 如果是第一次用到间接块，先分配存放间接表的块
            if (indirect_allocated_block == 0) {
                indirect_allocated_block = alloc_block();
                din.addrs[NDIRECT] = indirect_allocated_block;
                // 注意：这里我们还没写这个块，只是先占个座
            }
            // 更新内存中的间接表
            indirect[logic_block_idx - NDIRECT] = b;
        }

        din.size += n;
    }
    close(fd);

    // 3. 如果用到了间接块，把间接表写入磁盘
    if (indirect_allocated_block != 0) {
        write_block(indirect_allocated_block, (char*)indirect);
    }

    // 4. 写入文件 Inode
    write_inode(inum, &din);

    // 5. 将文件添加到根目录
    struct dirent de;
    memset(&de, 0, sizeof(de));
    de.inum = inum;
    strncpy(de.name, fs_name, DIRSIZ);

    // 计算偏移量
    // 根目录有: . (0), .. (1), console (2)
    // 所以第 1 个用户文件在 index 3
    // 这里我们依然利用 mkfs 的顺序执行特性，使用静态偏移量
    // 更好的做法是维护一个 root_dir_size 变量
    static int root_dir_entries = 3;
    off_t off = datastart_block * BSIZE + root_dir_entries * sizeof(struct dirent);

    // 写入目录项
    if (lseek(fsfd, off, SEEK_SET) != off) die("root dir: lseek failed");
    if (write(fsfd, &de, sizeof(de)) != sizeof(de)) die("root dir: write failed");
    root_dir_entries++;

    // 更新根目录大小
    struct dinode root_din;
    off_t root_off = inode_offset(ROOT_INODE);
    lseek(fsfd, root_off, SEEK_SET);
    read(fsfd, &root_din, sizeof(root_din));
    root_din.size += sizeof(struct dirent);
    write_inode(ROOT_INODE, &root_din);

    printf("Added: %-15s (inum %d, size %d bytes)\n", fs_name, inum, din.size);
}

// ==========================================
// Main
// ==========================================

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: mkfs fs.img [files...]\n");
        exit(1);
    }

    fsfd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (fsfd < 0) {
        perror("open");
        exit(1);
    }


    init_superblock();
    write_superblock();

    init_root_dir(); // 占用 datastart_block
    add_console_device(); // 仅添加 inode 和 dirent

    // 处理命令行传入的文件
    for (int i = 2; i < argc; i++) {
        char *path = argv[i];
        char *name = strrchr(path, '/');
        if (name) name++;
        else name = path;

        if (name[0] == '_') name++; // 去掉前缀 _

        append_user_program(path, name);
    }

    // 最后更新位图
    init_bitmap();

    fsync(fsfd);
    close(fsfd);
    printf("✅ mkfs: fs.img created successfully!\n");
    return 0;
}
