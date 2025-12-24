#include "../include/fs.h"
#include "../include/sysinfo.h"
#include "../include/param.h"
#include "../include/pipe.h"
#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/string.h"
#include "../include/syscall.h"
#include "../include/vm.h"
// 获取 fd 参数，并返回对应的 struct file 指针
// pfd: 可选，用来回传 fd 号
static int argfd(int n, int *pfd, struct file **pf) {
    int fd;
    struct file *f;
    struct proc *p = proc_running();

    // 获取第 n 个整数参数
    argint(n, &fd);

    // 校验 fd 范围
    if (fd < 0 || fd >= NOFILE || (f = p->open_file[fd]) == 0)
        return -1;

    if (pfd)
        *pfd = fd;
    if (pf)
        *pf = f;
    return 0;
}

// 在当前进程分配一个空闲 fd
static int fdalloc(struct file *f) {
    struct proc *p = proc_running();

    for (int fd = 0; fd < NOFILE; fd++) {
        if (p->open_file[fd] == 0) {
            p->open_file[fd] = f;
            return fd;
        }
    }
    return -1;
}

// 辅助：创建新 inode (用于 open O_CREATE)
static struct inode *create(char *path, short type, short major, short minor) {
    struct inode *ip, *dp;
    char name[DIRSIZ];

    // 1. 找父目录
    if ((dp = fs_nameiparent(path, name)) == 0)
        return 0;

    fs_inode_lock(dp);
    fs_inode_read(dp); // 读取父目录数据

    // 2. 检查文件是否已存在
    if ((ip = fs_dir_lookup(dp, name, 0)) != 0) {
        fs_inode_unlock(dp);
        fs_inode_release(dp);
        fs_inode_lock(ip); // 锁定找到的 inode
        fs_inode_read(ip);
        if (type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE)) {
            fs_inode_unlock(ip); //返回前必须解锁！
            return ip; // 存在且类型对，直接返回
        }
        fs_inode_unlock(ip);
        fs_inode_release(ip);
        return 0; // 类型冲突 (比如原本是个目录)
    }

    // 3. 分配新 inode
    if ((ip = fs_inode_alloc(dp->dev, type)) == 0) {
        panic("create: ialloc");
        return 0;
    }

    fs_inode_lock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->nlink = 1;
    fs_inode_write(ip); // 写回 inode (valid=1)
    fs_inode_unlock(ip);

    // 4. 链接到父目录
    if (fs_dir_link(dp, name, ip->inum) < 0)
        panic("create: dir_link");

    fs_inode_unlock(dp);
    fs_inode_release(dp); // 释放父目录
    return ip;
}

uint64 syscall_open(void) {
    char path[MAXPATH];
    int fd, omode;
    struct file *f = 0;
    struct inode *ip = 0;
    int n;
    int ret = -1;

    n = argstr(0, path, MAXPATH);

    // 1. 获取参数: path, mode
    if ((n) < 0 || argint(1, &omode) < 0)
        return -1;

    fslog_op_begin();

    // 2. 处理 O_CREATE
    if (omode & O_CREATE) {
        // 创建普通文件
        ip = create(path, T_FILE, 0, 0);
        if (ip == 0)
            goto out;
    } else {
        // 查找现有文件
        if ((ip = fs_namei(path)) == 0)
            goto out;

        fs_inode_lock(ip);
        fs_inode_read(ip); // 锁定/加载

        // 检查: 不能打开目录进行写操作
        if (ip->type == T_DIR && omode != O_RDONLY) {
            fs_inode_unlock(ip);
            fs_inode_release(ip);
            ip = 0;
            goto out;
        }
        fs_inode_unlock(ip);
    }

    // 3. 分配 struct file (内核层)
    if ((f = file_alloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f) file_close(f);
        if (ip) fs_inode_release(ip);
        goto out;
    }

    // 4. 填充 struct file
    if (ip->type == T_DEVICE) {
        f->type = FD_DEVICE;
        f->major = ip->major; // 从 inode 继承设备号
    } else {
        f->type = FD_INODE;
        f->off = 0;
    }
    f->off = 0;
    f->ip = ip;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

    ret = fd;

out:
    fslog_op_end();
    return ret;
}

// kernel/sysfile.c

uint64 syscall_close(void) {
    int fd;
    struct file *f;

    if (argfd(0, &fd, &f) < 0)
        return -1;

    struct proc *p = proc_running();
    p->open_file[fd] = 0; // 从进程表中移除
    file_close(f); // 减少引用计数 (可能触发释放)
    return 0;
}

uint64 syscall_read(void) {
    struct file *f;
    int n;
    uint64 p;

    // 参数: fd, buf, count
    if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
        return -1;

    return file_read(f, p, n);
}

uint64 syscall_write(void) {
    struct file *f;
    int n;
    uint64 p;

    // 现在的 sys_write 极其干净！
    // 所有的 fd=1 (控制台) 逻辑都已经被封装在 file_write -> console_write 里了
    if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
        return -1;

    fslog_op_begin();
    int r = file_write(f, p, n);
    fslog_op_end();
    return r;
    // struct file *f;
    // int n;
    // uint64 p;
    //
    //
    // uint64 buf_va;
    // int fd, count;
    // argint(0, &fd);
    // argint(2, &count);
    // argaddr(1, &buf_va);
    // struct proc *pr = proc_running();
    // if (fd == 1) {
    //     // 你的 uputc 会调用 write(1, &buf, 1)，所以 count 经常是 1
    //     // 但我们做一个健壮的实现，以防将来 uprintf 做了缓冲
    //
    //     // 在内核栈上创建一个临时小缓冲区
    //     char kernel_buf[64];
    //     int total_written = 0;
    //
    //     while (total_written < count) {
    //         // 计算本次循环要复制多少字节
    //         int n = (count - total_written < 64) ? (count - total_written) : 64;
    //
    //         // 3. 从用户空间复制数据到内核缓冲区
    //         if (vmem_copyin(pr->pagetable, kernel_buf, buf_va + total_written, n) < 0) {
    //             // 用户指针无效！
    //             break; // 停止写入，但返回已成功的部分
    //         }
    //
    //         // 4. 【功能】把内核缓冲区的内容打印到控制台
    //         for (int i = 0; i < n; i++) {
    //             console_putc(kernel_buf[i]); // 假设你有这个函数
    //             // printf("%c", kernel_buf[i]); // 或者直接用你内核的 printf
    //         }
    //
    //         total_written += n;
    //     }
    //     console_flush();
    //     return total_written; // 返回实际写入的字节数
    // }
    //
    // // 参数: fd, buf, count
    // if (argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    //     return -1;
    //
    //
    // // 🔥 兼容性处理：如果 fd 是 1 (stdout) 且还没有打开真正的控制台文件
    // // 我们可以保留一个后门，直接打到串口。
    // // 但更推荐的做法是：让 init 进程 open("/console") 得到 fd 0,1,2。
    // // 这里我们先走标准流程：
    // // 2. 只处理 fd=1 (stdout)
    // return file_write(f, p, n);
}

uint64 syscall_mkdir(void) {
    char path[MAXPATH];
    char name[DIRSIZ];
    struct inode *ip = 0;
    struct inode *dp = 0;
    int ret = -1;

    // 获取路径参数
    if (argstr(0, path, MAXPATH) < 0)
        return -1;

    fslog_op_begin();

    // 1. 创建 inode (类型为 T_DIR)
    ip = create(path, T_DIR, 0, 0);
    if (ip == 0)
        goto out;

    fs_inode_lock(ip);
    fs_inode_read(ip);
    // 新目录至少有 "." 和 ".." 两个链接
    ip->nlink = 2;
    fs_inode_write(ip);
    fs_inode_unlock(ip);

    // 2. 写入 . 指向自己
    if (fs_dir_link(ip, ".", ip->inum) < 0)
        panic("sys_mkdir: .");

    // 3. 写入 .. 指向父目录，并更新父目录 nlink
    dp = fs_nameiparent(path, name);
    if (dp == 0)
        goto out;

    fs_inode_lock(dp);
    fs_inode_read(dp);

    if (fs_dir_link(ip, "..", dp->inum) < 0)
        panic("sys_mkdir: ..");

    dp->nlink++; // 新目录占用了父目录的一个子目录引用
    fs_inode_write(dp);
    fs_inode_unlock(dp);

    ret = 0;

out:
    if (dp) fs_inode_release(dp);
    if (ip) fs_inode_release(ip);
    fslog_op_end();
    return ret;
}

uint64 syscall_link(void) {
    char oldpath[MAXPATH], newpath[MAXPATH], name[DIRSIZ];
    struct inode *ip = 0;
    struct inode *dp = 0;
    int ret = -1;

    if (argstr(0, oldpath, MAXPATH) < 0 || argstr(1, newpath, MAXPATH) < 0)
        return -1;

    fslog_op_begin();

    ip = fs_namei(oldpath);
    if (ip == 0)
        goto out;

    fs_inode_lock(ip);
    fs_inode_read(ip);
    if (ip->type == T_DIR) {
        fs_inode_unlock(ip);
        goto out;
    }
    ip->nlink++;
    fs_inode_write(ip);
    fs_inode_unlock(ip);

    dp = fs_nameiparent(newpath, name);
    if (dp == 0)
        goto revert;

    fs_inode_lock(dp);
    fs_inode_read(dp);
    if (dp->dev != ip->dev) {
        fs_inode_unlock(dp);
        goto revert;
    }

    if (fs_dir_link(dp, name, ip->inum) < 0) {
        fs_inode_unlock(dp);
        goto revert;
    }
    fs_inode_unlock(dp);

    ret = 0;
    goto out;

revert:
    fs_inode_lock(ip);
    fs_inode_read(ip);
    ip->nlink--;
    fs_inode_write(ip);
    fs_inode_unlock(ip);
out:
    if (dp) fs_inode_release(dp);
    if (ip) fs_inode_release(ip);
    fslog_op_end();
    return ret;
}

static int dir_is_empty(struct inode *ip) {
    struct dirent de;
    for (int off = 2 * sizeof(de); off < ip->size; off += sizeof(de)) {
        if (fs_inode_read_data(ip, 0, (char *) &de, off, sizeof(de)) != sizeof(de))
            panic("dir_is_empty: read");
        if (de.inum != 0) {
            return 0;
        }
    }
    return 1;
}

uint64 syscall_unlink(void) {
    char path[MAXPATH];
    char name[DIRSIZ];
    struct inode *dp = 0, *ip = 0;
    struct dirent de;
    uint off;
    int ret = -1;

    if (argstr(0, path, MAXPATH) < 0)
        return -1;

    fslog_op_begin();

    dp = fs_nameiparent(path, name);
    if (dp == 0)
        goto out;

    fs_inode_lock(dp);
    fs_inode_read(dp);

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        goto bad;

    ip = fs_dir_lookup(dp, name, &off);
    if (ip == 0)
        goto bad;

    fs_inode_lock(ip);
    fs_inode_read(ip);

    if (ip->nlink < 1)
        panic("syscall_unlink: nlink < 1");

    if (ip->type == T_DIR && !dir_is_empty(ip))
        goto bad;

    memset(&de, 0, sizeof(de));
    if (fs_inode_write_data(dp, 0, (char *) &de, off, sizeof(de)) != sizeof(de))
        panic("syscall_unlink: write");

    if (ip->type == T_DIR) {
        dp->nlink--;
        fs_inode_write(dp);
    }

    ip->nlink--;
    fs_inode_write(ip);
    ret = 0;

bad:
    if (ip) {
        fs_inode_unlock(ip);
        fs_inode_release(ip);
    }
    fs_inode_unlock(dp);
    fs_inode_release(dp);
out:
    fslog_op_end();
    return ret;
}

uint64 syscall_chdir(void) {
    char path[MAXPATH];
    struct inode *ip;
    struct proc *p = proc_running();
    fslog_op_begin();
    if (argstr(0, path, MAXPATH) < 0)
        goto bad;

    ip = fs_namei(path);
    if (ip == 0)
        goto bad;

    fs_inode_lock(ip);
    fs_inode_read(ip);
    if (ip->type != T_DIR) {
        fs_inode_unlock(ip);
        fs_inode_release(ip);
        goto bad;
    }
    fs_inode_unlock(ip);

    if (p->cwd) {
        fs_inode_release(p->cwd); // 释放旧的目录
    }
    p->cwd = ip;
    fslog_op_end();
    return 0;
bad:
    fslog_op_end();
    return -1;
}

uint64 syscall_fstat(void) {
    struct file *f;
    uint64 st; // 用户空间的 struct stat 指针

    if (argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
        return -1;

    return file_stat(f, st);
}

uint64 syscall_sysinfo(void) {
    struct sysinfo info;
    uint64 addr; // 用户传入的结构体指针

    if (argaddr(0, &addr) < 0)
        return -1;

    // 目前只支持主设备 ROOTDEV (1)
    // 未来可以扩展为支持传入 path 来查看特定挂载点
    fs_get_info(ROOTDEV, &info.total_blocks, &info.free_blocks, &info.total_inodes, &info.free_inodes);

    struct proc *p = proc_running();
    if (vmem_copyout(p->pagetable, addr, (char *) &info, sizeof(info)) < 0)
        return -1;

    return 0;
}

uint64 syscall_pipe(void) {
    uint64 fdarray; // 用户传入的数组指针 int fd[2]
    struct file *rf, *wf;
    int fd0, fd1;
    struct proc *p = proc_running();

    // 1. 获取参数
    if (argaddr(0, &fdarray) < 0)
        return -1;

    // 2. 分配 pipe 和两个 file
    if (pipe_alloc(&rf, &wf) < 0)
        return -1;

    // 3. 分配两个 fd
    fd0 = -1;
    if ((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0) {
        // 分配失败的回滚逻辑
        if (fd0 >= 0) p->open_file[fd0] = 0;
        file_close(rf);
        file_close(wf);
        return -1;
    }

    // 4. 将 fd 号拷贝回用户空间
    if (vmem_copyout(p->pagetable, fdarray, (char *) &fd0, sizeof(fd0)) < 0 ||
        vmem_copyout(p->pagetable, fdarray + sizeof(fd0), (char *) &fd1, sizeof(fd1)) < 0) {
        p->open_file[fd0] = 0;
        p->open_file[fd1] = 0;
        file_close(rf);
        file_close(wf);
        return -1;
    }

    return 0;
}
