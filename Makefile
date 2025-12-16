# 交叉工具链
TOOLPREFIX = riscv64-unknown-elf-

CC      = $(TOOLPREFIX)gcc
LD      = $(TOOLPREFIX)ld
AS      = $(TOOLPREFIX)as
OBJCOPY = $(TOOLPREFIX)objcopy

CFLAGS  = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -MD -mcmodel=medany
CFLAGS += -fno-builtin
CFLAGS += -Iinclude

LDFLAGS = -T kernel/kernel.ld

ifndef CPUS
CPUS := 1
endif

QEMUOPTS = \
    -machine virt \
    -m 128M \
    -nographic \
    -bios none \
    -smp $(CPUS) \
    -kernel kernel.elf

QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

# ============================
# 一、内核对象文件
# ============================

KOBJS = \
  kernel/entry.o \
  kernel/start.o \
  kernel/main.o \
  kernel/kalloc.o \
  kernel/vm.o \
  kernel/trap.o \
  kernel/kernelvec.o \
  kernel/plic.o \
  kernel/exec.o \
  kernel/proc.o \
  kernel/sem.o \
  kernel/swtch.o \
  kernel/syscall.o \
  kernel/trampoline.o \
  kernel/virtio_disk.o \
  kernel/fsbuf.o \
  kernel/fs.o \
  kernel/file.o \
  kernel/sysfile.o \
  kernel/fslog.o \
  kernel/pipe.o \
  kernel/uart.o \
  kernel/console.o \
  kernel/printf.o \
  kernel/string.o \
  kernel/timer.o \
  kernel/initcode.o \
  kernel/sleeplock.o \

# 默认目标：内核 + 文件系统镜像都生成
all: kernel.elf fs.img

# ----------------------------
# 内核的 .c / .S 编译规则
# ----------------------------

# 匹配 kernel/xxx.c -> kernel/xxx.o
kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 匹配 kernel/xxx.S -> kernel/xxx.o
kernel/%.o: kernel/%.S
	$(CC) $(CFLAGS) -c $< -o $@

# initcode 特殊：源码在 user/ 目录，但编译出的对象归内核使用
kernel/initcode.o: user/initcode.S
	$(CC) $(CFLAGS) -c $< -o $@

# 链接内核
kernel.elf: $(KOBJS)
	$(LD) $(LDFLAGS) -o $@ $(KOBJS)

# ============================
# 二、用户程序
# ============================

# 1. 要放进 fs.img 的用户 ELF 程序
UPROGS = \
  user/_init \
  user/_usertest \
  user/_fstest \
  user/_shell \
  user/_ls \
  user/_df \
  user/_logtest \
  user/_mkdir \
  user/_pipetest \
  user/_helloworld \
  user/_iosleeptest \
  user/_filetest \
  user/_semtest \
  user/_echo \

# 2. 所有用户程序共享的“用户库”对象
ULIB = \
  user/ulib/usys.o \
  user/ulib/printf.o \
  user/ulib/start.o \
  user/ulib/string.o \
  user/ulib/ulib.o \

# 3. 把 user 目录下的 .c / .S 编译成 .o
user/%.o: user/%.c
	$(CC) $(CFLAGS) -c $< -o $@

user/%.o: user/%.S
	$(CC) $(CFLAGS) -c $< -o $@

## 4. 通用规则：从 user/xxx.o + ULIB 链接出 user/_xxx（ELF）
user/_%: user/%.o $(ULIB)
	$(LD) -T user/ulib/user.ld -N -e _start -Ttext 0 -o $@ $^
	#$(OBJCOPY) --strip-all $@

# ============================
# 三、文件系统镜像
# ============================

# mkfs 工具
mkfs: mkfs.c
	gcc -Werror -Wall -o $@ $<

# 生成 fs.img：依赖 mkfs 和所有用户程序
fs.img: mkfs $(UPROGS)
	./mkfs fs.img $(UPROGS)

# ============================
# 四、运行 / 调试 / 清理
# ============================

qemu: kernel.elf fs.img
	qemu-system-riscv64 $(QEMUOPTS)

qemu-gdb: kernel.elf fs.img
	qemu-system-riscv64 $(QEMUOPTS) -s -S

clean:
	find . -name '*.o' -delete
	find . -name '*.d' -delete
	rm -f kernel.elf fs.img mkfs user/_*