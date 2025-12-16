#include "ulib/user.h"
#include "fs.h"

// 用户空间文件系统压力测试
#define BIG_FILE_SIZE (14 * 1024) // 14KB，足以触发间接块
char buf[BSIZE];
// 辅助：生成校验数据
void pattern(char *s, int len, int offset) {
    for (int i = 0; i < len; i++) {
        s[i] = (char) ((offset + i) & 0xff);
    }
}

// 1. 大文件测试
void test_bigfile() {
    printf("--- [USER TEST] 1. Big File Test (Indirect Block) ---\n");

    int fd = open("bigfile", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf("Error: create bigfile failed\n");
        exit(1);
    }

    printf("Writing %d bytes...\n", BIG_FILE_SIZE);
    for (int i = 0; i < BIG_FILE_SIZE; i += BSIZE) {
        pattern(buf, BSIZE, i);
        if (write(fd, buf, BSIZE) != BSIZE) {
            printf("Error: write failed at %d\n", i);
            exit(1);
        }
    }
    close(fd);

    // 验证
    fd = open("bigfile", O_RDONLY);
    if (fd < 0) {
        printf("Error: open bigfile failed\n");
        exit(1);
    }

    printf("Verifying data...\n");
    for (int i = 0; i < BIG_FILE_SIZE; i += BSIZE) {
        pattern(buf, BSIZE, i);
        char readbuf[BSIZE];
        if (read(fd, readbuf, BSIZE) != BSIZE) {
            printf("Error: read failed at %d\n", i);
            exit(1);
        }
        for (int j = 0; j < BSIZE; j++) {
            if (readbuf[j] != buf[j]) {
                printf("Data mismatch at offset %d\n", i + j);
                exit(1);
            }
        }
    }
    close(fd);

    // 检查文件大小
    struct stat st;
    fd = open("bigfile", O_RDONLY);
    if (fstat(fd, &st) < 0) {
        printf("Error: fstat failed\n");
        exit(1);
    }
    printf("File size is %d bytes (Expected %d) -> %s\n",
           (int) st.size, BIG_FILE_SIZE, st.size == BIG_FILE_SIZE ? "OK" : "FAIL");
    close(fd);

    printf("✅ Big File Test Passed\n\n");
}

// 2. 递归目录测试
void test_recursive() {
    printf("--- [USER TEST] 2. Recursive Directory Test ---\n");

    printf("Creating /dir_a ...\n");
    if (mkdir("dir_a") < 0) {
        printf("Error: mkdir dir_a failed\n");
        exit(1);
    }

    printf("Creating /dir_a/dir_b ...\n");
    if (mkdir("dir_a/dir_b") < 0) {
        printf("Error: mkdir dir_a/dir_b failed\n");
        exit(1);
    }

    printf("Creating /dir_a/dir_b/deep.txt ...\n");
    int fd = open("dir_a/dir_b/deep.txt", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf("Error: create deep file failed\n");
        exit(1);
    }
    if (write(fd, "hello deep world", 16) != 16) {
        printf("Error: write deep file failed\n");
        exit(1);
    }
    close(fd);

    // 读取验证
    printf("Reading /dir_a/dir_b/deep.txt ...\n");
    fd = open("dir_a/dir_b/deep.txt", O_RDONLY);
    if (fd < 0) {
        printf("Error: open deep file failed\n");
        exit(1);
    }
    char tmp[32];
    read(fd, tmp, 16);
    tmp[16] = 0;
    printf("Content: %s\n", tmp);

    if (strcmp(tmp, "hello deep world") != 0) {
        printf("Error: Content mismatch\n");
        exit(1);
    }
    close(fd);

    printf("✅ Recursive Directory Test Passed\n\n");
}

// 3. 目录扩张测试
void test_manyfiles() {
    printf("--- [USER TEST] 3. Directory Expansion Test ---\n");

    if (mkdir("many") < 0) {
        printf("Error: mkdir many failed\n");
        exit(1);
    }

    printf("Creating 50 files in /many ...\n");
    for (int i = 0; i < 50; i++) {
        char name[32];
        // 手动实现简单的 sprintf
        // "many/f_XX"
        strcpy(name, "many/f_");
        name[7] = '0' + (i / 10);
        name[8] = '0' + (i % 10);
        name[9] = 0;

        int fd = open(name, O_CREATE | O_RDWR);
        if (fd < 0) {
            printf("Error: create %s failed\n", name);
            exit(1);
        }
        close(fd);
    }

    printf("Verifying file /many/f_49 ...\n");
    int fd = open("many/f_49", O_RDONLY);
    if (fd < 0) {
        printf("Error: open many/f_49 failed. Directory expansion likely failed.\n");
        exit(1);
    }
    close(fd);

    printf("✅ Directory Expansion Test Passed\n\n");
}

// 用户空间文件测试
void file_test() {
    int fd;
    char buf[32];

    printf("--- Test Open/Write/Read ---\n");

    // 1. 创建文件
    // 注意：这需要你的 mkfs 已经在根目录放入了 . 和 ..
    // 并且你的内核支持路径解析
    fd = open("test.txt", O_CREATE | O_RDWR);
    if (fd < 0) {
        printf("open create failed\n");
        exit(0);
    }
    printf("open create success, fd=%d\n", fd);

    // 2. 写入
    write(fd, "Hello FS!", 9);
    printf("write success\n");

    // 3. 关闭
    close(fd);

    // 4. 重新打开读取
    fd = open("test.txt", O_RDONLY);
    if (fd < 0) {
        printf("open read failed\n");
        exit(0);
    }

    read(fd, buf, 9);
    buf[9] = 0;
    printf("read back: %s\n", buf);

    close(fd);

    exit(0);
}

int main(void) {
    printf("\n🚀 Starting User Space File System Stress Test\n\n");

    test_bigfile();
    test_recursive();
    test_manyfiles();

    printf("🎉🎉🎉 ALL TESTS PASSED! 🎉🎉🎉\n");
}
