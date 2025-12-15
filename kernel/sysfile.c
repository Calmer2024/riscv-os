#include "../include/types.h"
#include "../include/proc.h"
#include "../include/printf.h"
#include "../include/defs.h"

uint64
sys_write(void)
{
    int fd;
    uint64 p; // 用户空间 buffer 的虚拟地址
    int n;    // 要写入的字节数

    // 1. 从 trapframe 取参数（不会失败）
    argint(0, &fd);
    argaddr(1, &p);
    argint(2, &n);

    // 2. 只支持 stdout(1) / stderr(2)
    if(fd != 1 && fd != 2) {
        printf("sys_write: only supports stdout(1) and stderr(2)\n");
        return -1;
    }

    // 3. 基本健壮性检查
    if(n < 0)
        return -1;

    struct proc *proc = myproc();

    // 4. 从用户空间逐字节拷贝并输出
    for(int i = 0; i < n; i++) {
        char c;
        if(copyin(proc->pagetable, &c, p + i, 1) != 0)
            return -1;
        printf("%c", c);
    }

    return n;
}
