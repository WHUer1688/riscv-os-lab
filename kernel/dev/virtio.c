#include "dev/virtio.h"
#include "memlayout.h"
#include "lib/print.h"
#include "lib/lock.h"
#include "mem/pmem.h"
#include "mem/str.h"

static struct virtio_dev vdisk;
static spinlock_t lk_virtio;

// 读取MMIO寄存器
static inline uint32 virtio_read32(uint32 offset) {
    return *(volatile uint32*)(VIRTIO_BASE + offset);
}

// 写入MMIO寄存器
static inline void virtio_write32(uint32 offset, uint32 value) {
    *(volatile uint32*)(VIRTIO_BASE + offset) = value;
}

// 初始化VirtIO设备
void virtio_init(void) {
    spinlock_init(&lk_virtio, "virtio");
    
    // 检查magic值
    uint32 magic = virtio_read32(VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != 0x74726976) {  // "virt" in little-endian
        panic("virtio: invalid magic");
    }
    
    // 检查版本
    uint32 version = virtio_read32(VIRTIO_MMIO_VERSION);
    if (version != 2) {
        panic("virtio: unsupported version");
    }
    
    // 检查设备ID
    uint32 device_id = virtio_read32(VIRTIO_MMIO_DEVICE_ID);
    if (device_id != VIRTIO_ID_BLOCK) {
        panic("virtio: not a block device");
    }
    
    // 重置设备
    virtio_write32(VIRTIO_MMIO_STATUS, 0);
    
    // 设置状态：ACKNOWLEDGE
    virtio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);
    
    // 设置状态：DRIVER
    virtio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    
    // 读取设备特性（暂时不检查）
    virtio_write32(VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    // uint32 features = virtio_read32(VIRTIO_MMIO_DEVICE_FEATURES);  // 暂时不使用
    
    // 设置驱动特性（暂时设为0）
    virtio_write32(VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    virtio_write32(VIRTIO_MMIO_DRIVER_FEATURES, 0);
    
    // 设置状态：FEATURES_OK
    virtio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    
    // 检查FEATURES_OK是否被接受
    uint32 status = virtio_read32(VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        panic("virtio: features not accepted");
    }
    
    // 初始化队列0
    virtio_write32(VIRTIO_MMIO_QUEUE_SEL, 0);
    
    // 检查队列大小
    uint32 queue_num_max = virtio_read32(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (queue_num_max < VIRTIO_QUEUE_SIZE) {
        panic("virtio: queue too small");
    }
    
    // 设置队列大小
    virtio_write32(VIRTIO_MMIO_QUEUE_NUM, VIRTIO_QUEUE_SIZE);
    
    // 分配队列内存（描述符、可用环、已用环）
    uint64 desc_pa = (uint64)pmem_alloc(true);
    uint64 avail_pa = (uint64)pmem_alloc(true);
    uint64 used_pa = (uint64)pmem_alloc(true);
    
    if (!desc_pa || !avail_pa || !used_pa) {
        panic("virtio: failed to alloc queue memory");
    }
    
    memset((void*)desc_pa, 0, PGSIZE);
    memset((void*)avail_pa, 0, PGSIZE);
    memset((void*)used_pa, 0, PGSIZE);
    
    vdisk.queues[0].num = VIRTIO_QUEUE_SIZE;
    vdisk.queues[0].desc = (struct virtq_desc*)desc_pa;
    vdisk.queues[0].avail = (struct virtq_avail*)avail_pa;
    vdisk.queues[0].used = (struct virtq_used*)used_pa;
    vdisk.used_idx[0] = 0;
    
    // 设置队列描述符地址
    virtio_write32(VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32)desc_pa);
    virtio_write32(VIRTIO_MMIO_QUEUE_DESC_HIGH, (uint32)(desc_pa >> 32));
    
    // 设置可用环地址
    virtio_write32(VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32)avail_pa);
    virtio_write32(VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uint32)(avail_pa >> 32));
    
    // 设置已用环地址
    virtio_write32(VIRTIO_MMIO_QUEUE_USED_LOW, (uint32)used_pa);
    virtio_write32(VIRTIO_MMIO_QUEUE_USED_HIGH, (uint32)(used_pa >> 32));
    
    // 使能队列
    virtio_write32(VIRTIO_MMIO_QUEUE_READY, 1);
    
    // 设置状态：DRIVER_OK
    virtio_write32(VIRTIO_MMIO_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK);
    
    printf("virtio: disk initialized\n");
}

// 中断处理
void virtio_intr(void) {
    // 读取中断状态
    uint32 status = virtio_read32(VIRTIO_MMIO_INTERRUPT_STATUS);
    
    if (status) {
        // 确认中断
        virtio_write32(VIRTIO_MMIO_INTERRUPT_ACK, status);
        
        // 处理队列0的中断
        // 这里可以唤醒等待I/O完成的进程
    }
}

// 磁盘读写（支持1024字节块，需要两个512字节sector）
int virtio_disk_rw(uint32 blockno, void *buf, int write) {
    struct virtq *q = &vdisk.queues[0];
    static struct virtio_blk_req req[2];
    static uint8 status[2];
    uint32 sector = blockno * 2;  // 每个block包含2个sector
    
    spinlock_acquire(&lk_virtio);
    
    // 处理两个sector
    for (int i = 0; i < 2; i++) {
        // 准备请求
        req[i].type = write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
        req[i].reserved = 0;
        req[i].sector = sector + i;
        if (write) {
            memcpy(req[i].data, (char*)buf + i * 512, 512);
        }
        
        // 分配描述符链：desc[i*3] = 请求头, desc[i*3+1] = 数据, desc[i*3+2] = 状态
        uint16 idx = i * 3;
        
        // 描述符：请求头
        q->desc[idx].addr = (uint64)&req[i];
        q->desc[idx].len = sizeof(struct virtio_blk_req) - 512 - 1;  // 不包括data和status
        q->desc[idx].flags = VIRTQ_DESC_F_NEXT;
        q->desc[idx].next = idx + 1;
        
        // 描述符：数据
        q->desc[idx + 1].addr = write ? (uint64)req[i].data : (uint64)((char*)buf + i * 512);
        q->desc[idx + 1].len = 512;
        q->desc[idx + 1].flags = (write ? VIRTQ_DESC_F_WRITE : 0) | VIRTQ_DESC_F_NEXT;
        q->desc[idx + 1].next = idx + 2;
        
        // 描述符：状态
        q->desc[idx + 2].addr = (uint64)&status[i];
        q->desc[idx + 2].len = 1;
        q->desc[idx + 2].flags = VIRTQ_DESC_F_WRITE;
        q->desc[idx + 2].next = 0;
        
        // 添加到可用环
        uint16 avail_idx = q->avail->idx;
        q->avail->ring[avail_idx % VIRTIO_QUEUE_SIZE] = idx;
        __sync_synchronize();
        q->avail->idx = avail_idx + 1;
        
        // 通知设备
        virtio_write32(VIRTIO_MMIO_QUEUE_NOTIFY, 0);
        
        // 等待完成（轮询方式，简化实现）
        while (vdisk.used_idx[0] != q->used->idx) {
            // 检查中断状态
            uint32 int_status = virtio_read32(VIRTIO_MMIO_INTERRUPT_STATUS);
            if (int_status) {
                virtio_write32(VIRTIO_MMIO_INTERRUPT_ACK, int_status);
                vdisk.used_idx[0] = q->used->idx;
                break;
            }
        }
        
        // 读取数据（如果是读操作）
        if (!write) {
            memcpy((char*)buf + i * 512, req[i].data, 512);
        }
        
        // 检查状态
        if (status[i] != VIRTIO_BLK_S_OK) {
            spinlock_release(&lk_virtio);
            return -1;
        }
    }
    
    spinlock_release(&lk_virtio);
    return 0;
}

