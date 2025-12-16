//
// Created by czh on 2025/12/13.
//

#ifndef RISCV_OS_SYSINFO_H
#define RISCV_OS_SYSINFO_H
#include "types.h"

// 文件系统信息结构
struct sysinfo {
    uint64 total_blocks;
    uint64 free_blocks;
    uint64 total_inodes;
    uint64 free_inodes;
};

struct stat {
    int dev; // File system's disk device
    unsigned int ino; // Inode number
    short type; // Type of file
    short nlink; // Number of links to file
    unsigned long long size; // Size of file in bytes
};

#endif //RISCV_OS_SYSINFO_H