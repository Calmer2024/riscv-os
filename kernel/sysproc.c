#include "../include/types.h"
#include "../include/riscv.h"
#include "../include/proc.h"
#include "../include/syscall.h"
#include "../include/vm.h"
#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/printf.h"

// (这些函数在 syscall.c 中定义)
extern int fetch_arg_int(int n, int *ip);
extern int fetch_arg_addr(int n, uint64 *ip);

// 假设 vm.c 提供了 copy_from_user 和 copy_to_user
// int copy_from_user(void *kernel_dst, uint64 user_src, uint64 len);
// int copy_to_user(uint64 user_dst, void *kernel_src, uint64 len);

int sys_getpid(void) {
    return myproc()->pid;
}

// 响应你的 "sys_write" 示例
int sys_write(void) {
    int fd;
    uint64 buf_addr;
    int count;
    char k_buf[128]; // 内核缓冲区

    // 1. 提取参数
    if (fetch_arg_int(0, &fd) < 0 ||
        fetch_arg_addr(1, &buf_addr) < 0 ||
        fetch_arg_int(2, &count) < 0) {
        return -1;
    }

    // 2. 参数有效性检查
    // (这里只处理 stdout)
    if (fd != 1 || count < 0) {
        return -1;
    }
    
    // 3. 调用内核函数实现
    // (我们必须分块从用户空间拷贝)
    int total_written = 0;
    while(count > 0) {
        int n = (count > sizeof(k_buf)) ? sizeof(k_buf) : count;
        
        // **关键安全步骤**：从用户空间拷贝
        if(copy_from_user(k_buf, buf_addr + total_written, n) < 0) {
            return -1; // 无效地址
        }
        
        // 调用你们的 console.c/printf.c 中的输出函数
        // (这里我们用 printf，但你最终应该用 console_write)
        for(int i = 0; i < n; i++) {
            printf("%c", k_buf[i]);
        }
        
        count -= n;
        total_written += n;
    }
    
    return total_written;
}

int sys_exit(void) {
    int status;
    struct proc *p = myproc();

    if(fetch_arg_int(0, &status) < 0) {
        return -1;
    }
    
    // (关闭所有打开的文件...)
    
    acquire(&p->lock);
    
    p->state = ZOMBIE;
    p->exit_status = status;
    
    // 唤醒父进程 (如果它在 wait())
    if(p->parent) {
        wakeup(p->parent);
    }
    
    // 永久切换回调度器
    swtch(&p->context, &mycpu()->context);
    
    // 不会返回
    return 0;
}

int sys_wait(void) {
    uint64 status_addr; // 存退出码的用户地址
    struct proc *p = myproc();
    
    if(fetch_arg_addr(0, &status_addr) < 0) {
        return -1;
    }
    
    for(;;) { // 循环寻找子进程
        int have_kids = 0;
        struct proc *child;

        // 遍历进程表
        for(child = proc; child < &proc[NPROC]; child++) {
            if(child->parent != p) {
                continue; // 不是我的孩子
            }
            
            acquire(&child->lock); // 锁住子进程
            
            have_kids = 1;
            if(child->state == ZOMBIE) {
                // 找到了一个僵尸子进程！
                
                // 拷贝退出状态码到用户空间
                if(status_addr != 0 && 
                   copy_to_user(status_addr, (void*)&child->exit_status, sizeof(int)) < 0) {
                    release(&child->lock);
                    return -1; // 用户地址无效
                }
                
                int child_pid = child->pid;
                free_process(child); // 回收资源 (free_process 会释放锁)
                return child_pid;
            }
            
            release(&child->lock);
        }
        
        if(!have_kids) {
            // 没有任何子进程
            return -1;
        }
        
        // 有子进程，但它们还在运行
        // 休眠，等待子进程 exit()
        acquire(&p->lock);
        sleep(p, &p->lock); // 传入 p 作为休眠通道
        release(&p->lock);
    }
}

int sys_fork(void) {
    struct proc *p = myproc();
    struct proc *np;

    // 1. 分配新进程
    np = alloc_process();
    if(np == 0) {
        return -1; // 失败
    }
    
    // 2. 复制父进程的内存
    // (假设 vm.c 提供了 uvm_copy)
    // int uvm_copy(pagetable_t old, pagetable_t new, uint64 sz);
    np->pagetable = create_pagetable(); // 假设 vm.h 提供了
    if(uvm_copy(p->pagetable, np->pagetable, p->sz) < 0) {
        free_process(np); // (free_process 会释放锁)
        return -1;
    }
    np->sz = p->sz;

    // 3. 复制父进程的 trapframe
    *(np->trapframe) = *(p->trapframe);
    
    // 4. 设置子进程的返回值 (a0) 为 0
    np->trapframe->a0 = 0;
    
    // 5. 设置父子关系
    np->parent = p;
    
    // 6. 设为 RUNNABLE
    np->state = RUNNABLE;
    
    release(&np->lock); // (alloc_process 返回时持有的锁)
    
    return np->pid;
}

int sys_kill(void) {
    // 暂未实现
    return -1;
}

int sys_open(void) {
    // 暂未实现
    return -1;
}

int sys_close(void) {
    // 暂未实现
    return -1;
}

int sys_read(void) {
    // 暂未实现
    return -1;
}

int sys_sbrk(void) {
    // 暂未实现
    return -1;
}