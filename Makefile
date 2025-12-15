# 工具链定义
CC = riscv64-unknown-elf-gcc
AS = riscv64-unknown-elf-as
LD = riscv64-unknown-elf-ld
OBJDUMP = riscv64-unknown-elf-objdump

# 编译选项
# CFLAGS 和 LDFLAGS 中都添加 -g
CFLAGS = -g -Wall -O0 -mcmodel=medany -Iinclude -nostdlib -ffreestanding
ASFLAGS = -g # 汇编文件也加上 -g
LDFLAGS = -g -T kernel/kernel.ld -nostdlib


# 汇编源文件
S_SRCS = kernel/entry.S \
         kernel/kernelvec.S \
         kernel/swtch.S \
         kernel/trampoline.S

# C源文件
C_SRCS = kernel/main.c \
         kernel/printf.c \
         kernel/uart.c \
         kernel/console.c \
         kernel/pmm.c \
         kernel/vm.c \
         kernel/trap.c \
         kernel/timer.c \
         kernel/proc.c \
         kernel/spinlock.c \
         kernel/syscall.c \
         kernel/sysproc.c \
         kernel/sysfile.c \
         kernel/test.c \
         lib/string.c \

# 目标文件
OBJS = $(S_SRCS:.S=.o) $(C_SRCS:.c=.o)

VPATH = lib:kernel

# 包含目录
INCLUDE_DIR = include

# 默认目标
all: kernel.elf

# 生成并运行
qemu: kernel.elf
	qemu-system-riscv64 \
      -machine virt \
      -m 128M \
      -nographic \
      -kernel kernel.elf

# 生成并运行并调试
qemu-gdb: kernel.elf
	qemu-system-riscv64 \
      -machine virt \
      -m 128M \
      -nographic \
      -kernel kernel.elf \
      -s -S

# 创建包含目录（如果不存在）
$(INCLUDE_DIR):
	mkdir -p $(INCLUDE_DIR)

# 编译规则 - 每个目标文件一个规则
kernel/entry.o: kernel/entry.S
	$(AS) $(ASFLAGS) -c $< -o $@

kernel/main.o: kernel/main.c | $(INCLUDE_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

kernel/uart.o: kernel/uart.c | $(INCLUDE_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接
kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# 清理
clean:
	rm -f $(OBJS) kernel.elf

.PHONY: all clean