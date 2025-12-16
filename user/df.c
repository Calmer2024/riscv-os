//
// Created by czh on 2025/12/13.
//

#include "ulib/user.h"

int main(int argc, char *argv[]) {
    struct sysinfo info;

    if (sysinfo(&info) < 0) {
        printf("df: sysinfo failed\n");
        exit(1);
    }

    printf("Filesystem Usage:\n");
    printf("-----------------\n");

    // 块信息 (假设块大小 1024B = 1KB)
    printf("Blocks (Total): %d\n", (uint32) info.total_blocks);
    printf("Blocks (Free) : %d\n", (uint32) info.free_blocks);
    printf("Blocks (Used) : %d\n", (uint32) (info.total_blocks - info.free_blocks));

    // 计算百分比 (注意不要除零)
    int block_usage = 0;
    if (info.total_blocks > 0)
        block_usage = (info.total_blocks - info.free_blocks) * 100 / info.total_blocks;
    printf("Usage         : %d%%\n", block_usage);

    printf("\n");

    // Inode 信息
    printf("Inodes (Total): %d\n", (uint32) info.total_inodes);
    printf("Inodes (Free) : %d\n", (uint32) info.free_inodes);
    printf("Inodes (Used) : %d\n", (uint32) (info.total_inodes - info.free_inodes));

    int inode_usage = 0;
    if (info.total_inodes > 0)
        inode_usage = (info.total_inodes - info.free_inodes) * 100 / info.total_inodes;
    printf("Usage         : %d%%\n", inode_usage);

    exit(0);
}
