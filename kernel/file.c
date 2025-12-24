#include "../include/file.h"
#include "../include/sysinfo.h"
#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/vm.h"
#include "../include/pipe.h"

// 全局打开文件表
struct {
    // struct spinlock lock; // 单核简版暂时不用锁
    struct file file[NFILE];
} ftable;

// 全局设备表实例
struct devsw devsw[NDEV];


void file_init(void) {
    // 初始化锁
    // 清空表 (BSS段通常自动清零，不写循环也行)
}

// 分配一个 file 结构体 (ref = 1)，分配失败返回0
struct file *file_alloc(void) {
    struct file *f;

    // 遍历全局表找空位
    for (f = ftable.file; f < ftable.file + NFILE; f++) {
        if (f->ref == 0) {
            f->ref = 1;
            f->type = FD_NONE;
            return f;
        }
    }
    return 0; // 表满了
}

// 增加引用计数 (用于 dup 或 fork)
struct file *file_dup(struct file *f) {
    if (f->ref < 1)
        panic("file_dup");
    f->ref++;
    return f;
}

// 关闭文件，生命周期管理的终点。当引用计数归零时，它负责切断与底层资源的联系。
void file_close(struct file *f) {
    struct file ff;

    if (f->ref < 1)
        panic("file_close");

    f->ref--;
    if (f->ref > 0) {
        return; // 还有别人在用
    }

    // 引用结束，释放逻辑
    // 先把结构体拷贝出来，清空全局表里的槽位，再操作
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    // 锁内只改“索引结构”的状态，真正重操作放到锁外，用局部拷贝继续干活。
    // release(&ftable.lock);

    if (ff.type == FD_PIPE) {
        // 调用 pipeclose，传入当前是读端还是写端
        pipe_close(ff.pipe, ff.writable);
    } else if (ff.type == FD_INODE || ff.type == FD_DEVICE) {
        // 核心: 释放 inode 引用
        fs_inode_release(ff.ip);
    }
}

// 读取文件分发
// addr: 用户空间的缓冲区地址
// n: 读取字节数
int file_read(struct file *f, uint64 addr, int n) {
    int r = 0;

    if (f->readable == 0)
        return -1;

    if (f->type == FD_PIPE) {
        return pipe_read(f->pipe, addr, n);
    }
    if (f->type == FD_DEVICE) {
        // 设备读取：查表分发
        if (f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
            return -1;
        // 调用设备驱动的 read 函数 (1 表示这是用户地址)
        r = devsw[f->major].read(1, addr, n);
    }
    if (f->type == FD_INODE) {
        // 普通文件读取
        fs_inode_lock(f->ip);
        fs_inode_read(f->ip);
        r = fs_inode_read_data(f->ip, 1, (char *) addr, f->off, n);
        fs_inode_unlock(f->ip);
        // 更新偏移量
        if (r > 0)
            f->off += r;
    }

    return r;
}

// 写入文件分发
int file_write(struct file *f, uint64 addr, int n) {
    int r = 0;

    if (f->writable == 0)
        return -1;

    if (f->type == FD_PIPE) {
        return pipe_write(f->pipe, addr, n);
    }
    if (f->type == FD_DEVICE) {
        // 设备写入：查表分发
        if (f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
            return -1;
        // 调用设备驱动的 write 函数
        r = devsw[f->major].write(1, addr, n);
    }
    if (f->type == FD_INODE) {
        // 普通文件写入
        // 限制最大文件大小 (防止写爆磁盘)
        int max = ((MAXFILE * BSIZE) - f->off);
        if (n > max)
            n = max;

        fs_inode_lock(f->ip);
        fs_inode_read(f->ip);
        // fs_inode_write_data 需要支持 is_user_addr = 1
        r = fs_inode_write_data(f->ip, 1, (char *) addr, f->off, n);
        fs_inode_unlock(f->ip);
        if (r > 0)
            f->off += r;
    }

    return r == n ? n : -1;
}

// 获取文件状态
int file_stat(struct file *f, uint64 addr) {
    struct stat st;

    if (f->type == FD_INODE || f->type == FD_DEVICE) {
        fs_inode_lock(f->ip);
        fs_inode_read(f->ip);
        st.dev = f->ip->dev;
        st.ino = f->ip->inum;
        st.type = f->ip->type;
        st.nlink = f->ip->nlink;
        st.size = f->ip->size;
        fs_inode_unlock(f->ip);

        // 将 struct stat 拷贝到用户空间 addr
        struct proc *p = proc_running();
        if (vmem_copyout(p->pagetable, addr, (char *) &st, sizeof(st)) < 0)
            return -1;
        return 0;
    }
    return -1;
}
