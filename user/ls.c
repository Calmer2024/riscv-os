//
// Created by czh on 2025/12/13.
//

#include "fs.h"
#include "ulib/user.h"

// 格式化输出文件名
char* fmtname(char *path) {
    static char buf[DIRSIZ+1];
    char *p;

    // 找到最后一个 / 后面的部分
    for(p=path+strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    if(strlen(p) >= DIRSIZ)
        return p;

    memmove(buf, p, strlen(p));
    // 补空格，为了对齐好看
    memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
    buf[DIRSIZ] = 0;
    return buf;
}

void ls(char *path) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0){
        printf("ls: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        printf("ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // 根据类型处理
    switch(st.type){
        case T_FILE: // 普通文件直接打印
            printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
            break;

        case T_DIR: // 目录则遍历
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)){
                printf("ls: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf+strlen(buf);
            *p++ = '/'; // 拼接路径

            // 循环读取目录项
            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                if(de.inum == 0) // 空槽位跳过
                    continue;

                // 拼接完整路径用于 stat
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;

                if(stat(buf, &st) < 0){
                    printf("ls: cannot stat %s\n", buf);
                    continue;
                }
                // 打印：文件名 类型 Inode 大小
                printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, (int)st.size);
            }
            break;
    }
    close(fd);
}

//TODO 工作目录还没实现
int main(int argc, char *argv[]) {
    int i;

    if(argc < 2){
        ls("."); // 默认列出当前目录
    } else {
        for(i=1; i<argc; i++)
            ls(argv[i]);
    }
    exit(0);
}