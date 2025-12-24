#ifndef RISCV_OS_FILE_H
#define RISCV_OS_FILE_H

#include "types.h"
#include "fs.h" // 需要 struct inode 定义

// 文件类型
enum {
    FD_NONE = 0,
    FD_PIPE,   // 管道
    FD_INODE,  // 普通文件/目录
    FD_DEVICE  // 设备文件
};

// 打开的文件结构 (Open File)
struct file {
    int type;          // FD_INODE, FD_PIPE, FD_DEVICE
    int ref;           // 引用计数
    char readable;
    char writable;

    struct pipe *pipe; // 指向管道结构体的指针
    struct inode *ip;  // FD_INODE 类型对应的 inode
    uint off;          // 当前读写偏移量
    short major;       // 设备号 (用于 FD_DEVICE)
};


// 宏: 每个进程最大打开文件数
#define NOFILE 16
// 宏: 全局最大打开文件数 (系统级限制)
#define NFILE 100

#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400

#define NDEV 10  // 最大支持的设备类型数
#define CONSOLE 1 // 控制台的主设备号

// 设备切换表结构 (Device Switch Table)
// 让 file.c 可以调用不同设备的 read/write
struct devsw {
    int (*read)(int, uint64, int);
    int (*write)(int, uint64, int);
};

// 全局设备表
extern struct devsw devsw[];


// 函数声明
void file_init(void);
struct file* file_alloc(void);
struct file* file_dup(struct file *f);
void file_close(struct file *f);
int file_stat(struct file *f, uint64 addr);
int file_read(struct file *f, uint64 addr, int n);
int file_write(struct file *f, uint64 addr, int n);

#endif //RISCV_OS_FILE_H