# 代码段
.section .text
.global _start

# 入口名称为 _start

_start:
    # 输出'S'表示启动成功
    li t0, 0x10000000     # UART基地址
    li t1, 'S'           
    sb t1, 0(t0)         # 0x10000000是发送数据寄存器的地址，将字符存在该寄存器里，可以将信号以串行数据输出

    # 设置栈指针
    la sp, stack_top      # 加载栈顶地址
    li t1, 'P'          
    sb t1, 0(t0)          # 输出字符P到串口来表示栈设置完成

    # 清零BSS段
    la a0, _bss_start     # 输入清零函数的参数
    la a1, _bss_end       
    call clear_bss        # 调用清零函数

    li t1, 'B'            # BSS清零标记
    sb t1, 0(t0)          # 输出字符B到串口来表示bss段清零完成

    call main             # 调用C语言主函数
	
    li t1,'E'		# 用E表示错误
    sb t1,0(t0)
    # 死循环保证安全性
spin:
    j spin

# BSS段清零函数
clear_bss:
    beq a0, a1, clear_done  # 如果起始地址等于结束地址，跳过
clear_loop:
    sb zero, 0(a0)          
    addi a0, a0, 1          # 移动到下一个字节
    blt a0, a1, clear_loop  # 如果还没到结束地址，继续循环
clear_done:
    ret                     # 返回
