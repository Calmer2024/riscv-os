//
// Created by czh on 2025/12/15.
//

#include "ulib/user.h"

int main(int argc, char *argv[]) {
    if(argc < 2){
        printf("Usage: logtest [1|2]\n");
        exit(0);
    }

    int type = atoi(argv[1]);

    printf("--- Log Test Start (Type %d) ---\n", type);

    // 1. 设置内核崩溃模式
    fslog_crash(type);

    // 2. 执行一个涉及多元数据修改的操作
    // 创建目录会修改: 父目录数据块, 新目录Inode, Bitmap, 新目录的数据块(..链接)
    if (type == 1) {
        printf("Attempting to create /dir_fail (Expect: Disappear after reboot)\n");
        mkdir("dir_fail");
    } else if (type == 2) {
        printf("Attempting to create /dir_success (Expect: Exist after reboot)\n");
        mkdir("dir_success");
    }

    printf("❌ FAILED: System should have crashed but didn't!\n");
    exit(0);
}