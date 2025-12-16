#ifndef RISCV_OS_PARAM_H
#define RISCV_OS_PARAM_H

#define MAX_PROCESS 64

// 文件系统块缓冲区大小
#define MAXOPBLOCKS  100  // max # of blocks any FS op writes
#define LOGBLOCKS (MAXOPBLOCKS*3)  // max data blocks in on-disk log
#define FSBUF_NUM (MAXOPBLOCKS*3) // 为什么是30？先不管
#define NINODE       50  // 缓存的活跃inodes数量
// 文件系统块数
#define FSSIZE 2000

#define MAXPATH      128
#define MAXARG       32  // max exec arguments

#endif //RISCV_OS_PARAM_H
