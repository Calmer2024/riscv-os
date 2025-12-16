# user/initcode.S
#include "syscall.h"
    .section .initcode, "ax"

start:
    # 1. 准备 exec 的参数
    la a0, init_path      # 参数1 (path): 加载字符串 "/init" 的地址
    la a1, argv           # 参数2 (argv): 加载指针数组 argv 的地址

    # 2. 调用 exec 系统调用
    li a7, SYSCALL_exec
    ecall

# 3. 正常情况下 exec 不会返回。如果返回了，说明出错了 (比如找不到文件)
#    为了防止跑飞，我们在这里死循环，或者调用 exit
exit:
    li a7, SYSCALL_exit
    ecall
    jal exit              # 如果 exit 也失败了，死循环

# ------------------------------------
# 数据区域 (紧跟在代码后面，一同加载到内存)
# ------------------------------------

# 保持 8 字节对齐
.p2align 3
argv:
    .dword init_path - start      # argv[0]: 指向下面的 init_path
    .dword 0              # argv[1]: NULL 结束符 (必需!)

init_path:
    .string "/init\0"     # 对应的文件路径