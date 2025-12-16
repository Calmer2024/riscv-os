#include "../include/fs.h"
#include "../include/kalloc.h"
#include "../include/memlayout.h"
#include "../include/printf.h"
#include "../include/proc.h"
#include "../include/riscv.h"
#include "../include/string.h"
#include "../include/types.h"
#include "../include/virtio.h" // VIRTIO_MMIO_BASE 等宏定义

int DEBUG = 0;

#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

struct fsbuf *current_disk_buf;

// 驱动本身的状态，虽然有NUM个但是现在只用一个
static struct {
    struct virtq_desc *desc; // 描述符表
    struct virtq_avail *avail; // 请求队列
    struct virtq_used *used; // 完成队列

    // 记录当前正在进行的请求状态（VIRTIO协议要求分三段）
    struct virtio_blk_req req;

    uint8 status; // 磁盘操作结果（成功/失败）
} disk;

void virtio_disk_init(void) {
    // 依次检查：
    // MAGIC_VALUE 是否为 'virt'（0x74726976）
    // 版本号是否为 2
    // DEVICE_ID 是否为 2（block device）
    // VENDOR_ID 是否为 QEMU 的值
    // 任一不符，说明 没有 virtio 磁盘设备，直接 panic。
    if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
        *R(VIRTIO_MMIO_VERSION) != 2 ||
        *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
        *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
        printf("DEBUG: Magic=0x%x, Version=0x%x, DevID=0x%x, VendorID=0x%x\n",
               *R(VIRTIO_MMIO_MAGIC_VALUE),
               *R(VIRTIO_MMIO_VERSION),
               *R(VIRTIO_MMIO_DEVICE_ID),
               *R(VIRTIO_MMIO_VENDOR_ID));
        panic("could not find virtio disk");
    }

    // 写 0 到 STATUS 寄存器，相当于重置设备。
    uint32 status = 0;
    // 初始化相关寄存器
    // 告诉设备：我们知道你在哪，我们准备好了
    *R(VIRTIO_MMIO_STATUS) = status;
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    // 2. Feature 功能协商
    // 从设备读取它支持的 feature bitmask
    uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    // 一条条清掉我们不想启用的特性
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    // 把“我认可/选择使用的 feature 集”写回设备。
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

    // 写 FEATURES_OK 状态，告诉设备“协商结束”。
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    // 再读一次 STATUS 看设备是否也认同这些配置，否则 panic。
    status = *R(VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_CONFIG_S_FEATURES_OK)) {
        printf("DEBUG: status: 0x%x\n", status);
        printf("VIRTIO_CONFIG_S_FEATURES_OK: 0x%x\n", VIRTIO_CONFIG_S_FEATURES_OK);
        panic("virtio disk FEATURES_OK unset");
    }

    // 初始化队列0，选择 队列 0，因为这个驱动只使用一个队列。
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
    // 确认队列 0 之前没被用过（READY 应该为 0）
    if (*R(VIRTIO_MMIO_QUEUE_READY))
        panic("virtio disk should not be ready");
    // 查看设备支持的最大 queue 长度 max
    // 为 0：说明设备不支持这个队列，panic。
    // 小于 NUM：我们需要的描述符数比设备支持的还多，不行，panic。
    uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
        panic("virtio disk has no queue 0");
    if (max < NUM)
        panic("virtio disk max queue too short");


    // 分配队列和描述的空间
    disk.desc = kmem_alloc();
    disk.avail = kmem_alloc();
    disk.used = kmem_alloc();
    if (!disk.desc || !disk.avail || !disk.used)
        panic("virtio disk kalloc");
    memset(disk.desc, 0, PAGE_SIZE);
    memset(disk.avail, 0, PAGE_SIZE);
    memset(disk.used, 0, PAGE_SIZE);

    // 告诉磁盘操作的物理地址
    *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64) disk.desc;
    *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64) disk.desc >> 32;
    *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64) disk.avail;
    *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64) disk.avail >> 32;
    *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64) disk.used;
    *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64) disk.used >> 32;

    // 告诉设备：这个队列我们打算用 NUM 个描述符槽位。
    *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

    // 标记队列 0 为 ready，可被设备使用。
    *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

    // 准备好了
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    printf("virtio_disk_init: initialized.\n");
}

// 核心函数：读写磁盘
// b->dev, b->blockno, b->data 已经准备好了
void virtio_disk_rw(struct fsbuf *b, int write) {
    uint64 sector = b->blockno * (BSIZE / 512); // xv6块转扇区号

    // --- 步骤 1: 填充 3 个描述符 (Descriptor) ---
    // VIRTIO 规定一个磁盘请求必须包含三个部分链在一起：
    // Desc[0]: 请求头 (Header) -> 告诉磁盘我要读/写哪个扇区
    // Desc[1]: 数据 (Data)     -> 真正的数据存放地址
    // Desc[2]: 状态 (Status)   -> 磁盘写回在这里，告诉我成功没

    // 1.1 填充头 (Desc[0])
    disk.req.type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    disk.req.sector = sector;

    disk.desc[0].addr = (uint64) &disk.req; // 物理地址
    disk.desc[0].len = sizeof(disk.req);
    disk.desc[0].flags = VRING_DESC_F_NEXT; // 还有下文
    disk.desc[0].next = 1; // 下一个是 Desc[1]

    // 1.2 填充数据 (Desc[1])
    disk.desc[1].addr = (uint64) b->data; // 缓冲区的数据地址
    disk.desc[1].len = BSIZE;
    if (write) {
        disk.desc[1].flags = VRING_DESC_F_NEXT; // 如果是写，设备只是读这块内存
    } else {
        disk.desc[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE; // 如果是读，设备要写这块内存
    }
    disk.desc[1].next = 2; // 下一个是 Desc[2]

    // 1.3 填充状态 (Desc[2])
    disk.desc[2].addr = (uint64) &disk.status;
    disk.desc[2].len = 1;
    disk.desc[2].flags = VRING_DESC_F_WRITE; // 设备会写这里
    disk.desc[2].next = 0; // 链条结束

    // --- 步骤 2: 把任务加入 "Available Ring" ---

    // 告诉设备：这一串描述符的头是 Desc[0]
    disk.avail->ring[disk.avail->idx % 8] = 0;

    __sync_synchronize(); // 内存屏障：确保上面数据都写好了再更新 idx

    disk.avail->idx += 1; // 任务数 +1

    current_disk_buf = b; // 保存当前正在处理的 buf，在中断里找回

    __sync_synchronize();

    // --- 步骤 3: 敲门通知设备 (Notify) ---
    b->disk = 1; // 1 表示提交给磁盘了，任务还没完成

    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // 0号队列有新消息！

    // --- 步骤 4: 死循环等待 (Busy Wait) ---
    // 真实 OS 会在这里 sleep(b)，然后中断里 wakeup
    // 咱们简化版就在这死等，直到 Used Ring 的 idx 追上 Avail Ring

    // 极其简化的判断：一直在轮询 used->idx 是否变化
    // 实际代码需要记录之前的 used_idx 来对比
    // 使用volatile 关键字，告诉编译器不要优化这段代码，
    // 否则会被优化成从寄存器里面取值，永远不会变
    // while (((volatile struct virtq_used *) disk.used)->idx != disk.avail->idx) {
    //     // 傻等...
    // }
    if (proc_running() == 0) {
        // 启动阶段：没有进程，只能忙等待
        while (((volatile struct virtq_used *) disk.used)->idx != disk.avail->idx) {
            // 忙等待...
        }
        // --- 模拟中断处理 ---
        // 因为这时候 virtio_disk_intr 可能不会跑，我们需要手动做清理工作
        *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;
        // 标记 buffer 完成
        b->disk = 0;
        b->valid = 1;
    } else {
        if (DEBUG)
            printf("RW: Start sleep...\n");
        while (b->disk == 1) {
            sleep(b); // 核心改变：交出 CPU
        }
        if (DEBUG)
            printf("RW: Woke up!\n");
        // --- 步骤 5: 收尾 ---
        // b->disk = 0; // 清除脏标记（如果是写操作）
        b->valid = 1; // 标记数据有效（如果是读操作）
    }
}

void virtio_disk_intr() {
    // 1. 告诉设备：中断已经被响应了 (ACK)
    if (DEBUG)
        printf("IRQ: Disk interrupt!\n");
    *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;
    __sync_synchronize();
    // 获取完成的任务 ID (在极简驱动里，我们需要一种方式把 ID 映射回 fsbuf)
    // 难点：我们需要知道这个完成的任务对应哪个 buf。
    // xv6 的做法是：desc[0].addr 存的不只是地址，它把 buf 的索引作为 ID 传给磁盘。
    // 既然我们现在是单进程串行排队（一次只提交一个），
    // 那么完成的肯定就是当前正在睡觉的那个 buf
    // 我们可以定义一个全局变量 struct fsbuf *current_disk_buf;
    // TODO：可以遍历 fsbuf找到对应的buf
    if (current_disk_buf) {
        // 先修改状态，确保唤醒看到的是任务已完成
        current_disk_buf->disk = 0;
        wakeup(current_disk_buf);
        current_disk_buf = 0;
    }
}


// ================= 磁盘驱动测试代码 =================

// 这是一个伪造的 buf，因为我们还没有 bio 层
// 我们手动分配一块内存给它
static struct fsbuf b;
static uchar data_buffer[BSIZE]; // 1024字节

void virtio_disk_test(void) {
    printf("--- [TEST] Start VirtIO Disk Test ---\n");

    // 1. 准备测试数据：写入 0, 1, 2, ...
    for (int i = 0; i < BSIZE; i++) {
        data_buffer[i] = (uchar) (i % 255);
    }

    // 2. 初始化 buf 结构
    b.dev = 1; // 这里的 dev 其实在极简驱动里没用到，写个1意思一下
    b.blockno = 1; // 重要：不要写第0块，那是超级块或者引导块，写第1块比较安全
    b.valid = 0; // 还没读
    b.disk = 1; // 脏了，需要写
    memmove(b.data, data_buffer, BSIZE); // 把数据拷进去

    printf("--- [TEST] Step 1: Writing pattern to sector 1...\n");
    // 调用驱动写入
    virtio_disk_rw(&b, 1); // 1 = write

    printf("--- [TEST] Write done. Clearing buffer...\n");

    // 3. 清除内存里的数据，制造“失忆”现场
    memset(b.data, 0, BSIZE);
    b.valid = 0; // 标记无效，虽然极简驱动不看这个，但保持语义一致

    printf("--- [TEST] Step 2: Reading back from sector 1...\n");
    // 调用驱动读取
    virtio_disk_rw(&b, 0); // 0 = read

    // 4. 验证数据
    printf("--- [TEST] Read done. Verifying data...\n");
    int error = 0;
    for (int i = 0; i < BSIZE; i++) {
        if (b.data[i] != (uchar) (i % 255)) {
            printf("[ERROR] Mismatch at byte %d: expected %d, got %d\n",
                   i, i % 255, b.data[i]);
            error = 1;
            break;
        }
    }

    if (!error) {
        printf("✅ [SUCCESS] VirtIO Disk Read/Write Test Passed!\n");
    } else {
        panic("❌ [FAIL] Disk Data Mismatch");
    }
}
