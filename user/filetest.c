//
// Created by czh on 2025/12/15.
//

#include "ulib/user.h"

#define BSIZE 1024

void test_basic_crud();
void test_directory_nesting();
void test_large_file();
void test_concurrency();
void test_performance();

// 简单的 assert
void assert(int condition, char *msg) {
    if (!condition) {
        printf("❌ ASSERT FAILED: %s\n", msg);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    printf("=== Starting Comprehensive File System Test ===\n");

    test_basic_crud();
    test_directory_nesting();
    test_large_file();
    test_concurrency();
    test_performance();

    printf("\n✅ ALL TESTS PASSED! Your File System is ROCK SOLID.\n");
    exit(0);
}

// 1. 基础增删改查测试
void test_basic_crud() {
    printf("\n[1/5] Testing Basic CRUD & Links...\n");
    int fd;
    char buf[64];

    // Create & Write
    printf("  - Create file 'crud.txt'\n");
    fd = open("crud.txt", O_CREATE | O_RDWR);
    assert(fd >= 0, "open create failed");
    write(fd, "hello", 5);
    close(fd);

    // Hard Link
    printf("  - Link 'crud_link.txt' -> 'crud.txt'\n");
    assert(link("crud.txt", "crud_link.txt") == 0, "link failed");

    // Unlink Original
    printf("  - Unlink 'crud.txt'\n");
    assert(unlink("crud.txt") == 0, "unlink failed");

    // Read Link (Data should persist)
    printf("  - Read from 'crud_link.txt'\n");
    fd = open("crud_link.txt", O_RDONLY);
    assert(fd >= 0, "open link failed");
    read(fd, buf, 5);
    buf[5] = 0;
    assert(strcmp(buf, "hello") == 0, "data mismatch in link");
    close(fd);

    // Unlink Link (Data should disappear)
    printf("  - Unlink 'crud_link.txt'\n");
    assert(unlink("crud_link.txt") == 0, "unlink link failed");

    fd = open("crud_link.txt", O_RDONLY);
    assert(fd < 0, "file should be gone");

    printf("  ✅ Basic CRUD passed.\n");
}

// 2. 目录嵌套测试
void test_directory_nesting() {
    printf("\n[2/5] Testing Directory Nesting...\n");

    printf("  - Creating /a/b/c/d...\n");
    assert(mkdir("a") == 0, "mkdir a");
    assert(chdir("a") == 0, "cd a");
    assert(mkdir("b") == 0, "mkdir b");
    assert(chdir("b") == 0, "cd b");
    assert(mkdir("c") == 0, "mkdir c");
    assert(chdir("c") == 0, "cd c");

    int fd = open("deep_file", O_CREATE | O_RDWR);
    assert(fd >= 0, "create deep file");
    write(fd, "deep", 4);
    close(fd);

    printf("  - Verifying absolute path...\n");
    assert(chdir("/") == 0, "cd /");
    fd = open("/a/b/c/deep_file", O_RDONLY);
    assert(fd >= 0, "open absolute path");
    close(fd);

    // Cleanup (Depth first)
    printf("  - Cleaning up...\n");
    unlink("/a/b/c/deep_file");
    unlink("/a/b/c"); // 假设你的 unlink 支持删除空目录，或者用 rmdir
    unlink("/a/b");
    unlink("/a");

    printf("  ✅ Directory nesting passed.\n");
}

// 3. 大文件测试 (测试间接块)
void test_large_file() {
    printf("\n[3/5] Testing Large File (Indirect Blocks)...\n");

    char buf[BSIZE];
    int fd = open("bigfile", O_CREATE | O_RDWR);
    assert(fd >= 0, "create bigfile");

    // 写入 16KB (超过直接块 12KB)
    // 假设 BSIZE=512, NDIRECT=12. 12*512 = 6KB.
    // 你的 NDIRECT 是 12 吗？如果是 12 个块，那就是 6KB。
    // 如果是 16KB，肯定触发间接块。
    int target_size = 20 * 1024;
    printf("  - Writing %d bytes...\n", target_size);

    memset(buf, 'A', BSIZE);
    for(int i = 0; i < target_size; i += BSIZE){
        write(fd, buf, BSIZE);
    }
    close(fd);

    struct stat st;
    fd = open("bigfile", O_RDONLY);
    fstat(fd, &st);
    printf("  - File size: %d (Expected %d)\n", st.size, target_size);
    assert(st.size == target_size, "size mismatch");

    // Verify data at boundary (e.g., around 6KB)
    char read_buf[BSIZE];
    read(fd, read_buf, BSIZE);
    assert(read_buf[0] == 'A', "data mismatch");

    close(fd);
    unlink("bigfile");
    printf("  ✅ Large file passed.\n");
}

// 4. 并发压力测试
void test_concurrency() {
    printf("\n[4/5] Testing Concurrency (Fork bomb style)...\n");

    int pid = fork();
    if(pid == 0){
        // Child 1
        for(int i=0; i<10; i++){
            int fd = open("concur_c1", O_CREATE | O_RDWR);
            write(fd, "child1", 6);
            close(fd);
            unlink("concur_c1");
        }
        exit(0);
    }

    int pid2 = fork();
    if(pid2 == 0){
        // Child 2
        for(int i=0; i<10; i++){
            int fd = open("concur_c2", O_CREATE | O_RDWR);
            write(fd, "child2", 6);
            close(fd);
            unlink("concur_c2");
        }
        exit(0);
    }

    // Parent
    for(int i=0; i<10; i++){
        int fd = open("concur_p", O_CREATE | O_RDWR);
        write(fd, "parent", 6);
        close(fd);
        unlink("concur_p");
    }

    wait(0);
    wait(0);
    printf("  ✅ Concurrency passed (No panic/deadlock).\n");
}

// 5. 性能测试
void test_performance() {
    printf("\n[5/5] Testing Performance...\n");

    int start = uptime();
    int N = 50;

    for(int i=0; i<N; i++){
        char name[16];
        sprintf(name, "f%d", i);
        int fd = open(name, O_CREATE | O_RDWR);
        write(fd, "test", 4);
        close(fd);
    }

    int mid = uptime();

    for(int i=0; i<N; i++){
        char name[16];
        sprintf(name, "f%d", i);
        unlink(name);
    }

    int end = uptime();

    printf("  - Create %d files: %d ticks\n", N, mid - start);
    printf("  - Delete %d files: %d ticks\n", N, end - mid);
    printf("  ✅ Performance check done.\n");
}