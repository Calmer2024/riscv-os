# 工具链定义
CC = riscv64-unknown-elf-gcc
AS = riscv64-unknown-elf-as
LD = riscv64-unknown-elf-ld
OBJDUMP = riscv64-unknown-elf-objdump

# 编译选项
CFLAGS = -Wall -O0 -mcmodel=medany -Iinclude
ASFLAGS = 
LDFLAGS = -T kernel/kernel.ld -nostdlib

# 目标文件
OBJS = kernel/entry.o kernel/main.o kernel/uart.o

# 包含目录
INCLUDE_DIR = include

# 默认目标
all: kernel.elf

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
