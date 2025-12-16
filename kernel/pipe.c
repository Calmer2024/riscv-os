#define PIPESIZE 512
#include "../include/file.h"
#include "../include/kalloc.h"
#include "../include/proc.h"
#include "../include/vm.h"

struct pipe {
    char data[PIPESIZE];
    uint nread; // 读出的总字节数
    uint nwrite; // 写入的总字节数
    int readopen; // 读端是否打开
    int writeopen; // 写端是否打开
};

// 分配一个管道，并填充两个 file 结构体
// f0: 读端, f1: 写端
int pipe_alloc(struct file **f0, struct file **f1) {
    struct pipe *pi;

    pi = 0;
    *f0 = *f1 = 0;

    // 1. 分配 file 结构
    if ((*f0 = file_alloc()) == 0 || (*f1 = file_alloc()) == 0)
        goto bad;

    // 2. 分配 pipe 内存
    if ((pi = (struct pipe *) kmem_alloc()) == 0)
        goto bad;

    pi->readopen = 1;
    pi->writeopen = 1;
    pi->nwrite = 0;
    pi->nread = 0;

    // 3. 关联 file -> pipe
    (*f0)->type = FD_PIPE;
    (*f0)->readable = 1;
    (*f0)->writable = 0;
    (*f0)->pipe = pi;

    (*f1)->type = FD_PIPE;
    (*f1)->readable = 0;
    (*f1)->writable = 1;
    (*f1)->pipe = pi;

    return 0;

bad:
    if (pi) kmem_free((char *) pi);
    if (*f0) file_close(*f0);
    if (*f1) file_close(*f1);
    return -1;
}


// 关闭管道的一端
void pipe_close(struct pipe *pi, int writable) {
    if(writable){
        pi->writeopen = 0;
        wakeup(&pi->nread); // 唤醒读进程：告诉它 EOF 了
    } else {
        pi->readopen = 0;
        wakeup(&pi->nwrite); // 唤醒写进程：告诉它读端关闭管道了
    }

    // 如果两端都关了，释放内存
    if(pi->readopen == 0 && pi->writeopen == 0){
        kmem_free((char*)pi);
    } else {
        // release(&pi->lock);
    }
}
// 写管道
// addr: 用户缓冲区地址
// n: 写入字节数
int pipe_write(struct pipe *pi, uint64 addr, int n) {
    int i = 0;
    struct proc *pr = proc_running();
    // acquire(&pi->lock);
    while(i < n){
        // 1. 检查读端是否关闭
        if(pi->readopen == 0){
            // release(&pi->lock);
            return -1; // Broken Pipe
        }

        // 2. 检查缓冲区是否满了
        if(pi->nwrite == pi->nread + PIPESIZE){
            // 唤醒读者，自己睡觉
            wakeup(&pi->nread);
            sleep(&pi->nwrite);
        } else {
            // 3. 写入一个字节
            char ch;
            if(vmem_copyin(pr->pagetable, &ch, addr + i, 1) == -1)
                break;

            pi->data[pi->nwrite++ % PIPESIZE] = ch;
            i++;
        }
    }

    // 写完了，唤醒读者
    wakeup(&pi->nread);
    // release(&pi->lock);
    return i;
}


// 读管道
int pipe_read(struct pipe *pi, uint64 addr, int n) {
    int i = 0;
    struct proc *pr = proc_running();
    char ch;

    // acquire(&pi->lock);

    // 1. 如果缓冲区空，且写端还开着 -> 等待
    while(pi->nread == pi->nwrite && pi->writeopen){
        // if(proc_killed(pr)){ // 响应 kill
        //     release(&pi->lock);
        //     return -1;
        // }
        sleep(&pi->nread);
    }

    // 2. 读取数据
    for(i = 0; i < n; i++){
        if(pi->nread == pi->nwrite)
            break; // 读空了

        ch = pi->data[pi->nread++ % PIPESIZE];

        if(vmem_copyout(pr->pagetable, addr + i, &ch, 1) == -1)
            break;
    }

    // 读完了，唤醒写者（如果有位置了）
    wakeup(&pi->nwrite);
    // release(&pi->lock);
    return i;
}