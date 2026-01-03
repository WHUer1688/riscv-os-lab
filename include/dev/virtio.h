#ifndef __VIRTIO_H__
#define __VIRTIO_H__

#include "common.h"
#include "memlayout.h"

// VirtIO MMIO 寄存器偏移
#define VIRTIO_MMIO_MAGIC_VALUE        0x000
#define VIRTIO_MMIO_VERSION            0x004
#define VIRTIO_MMIO_DEVICE_ID          0x008
#define VIRTIO_MMIO_VENDOR_ID          0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES    0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES    0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL          0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX      0x034
#define VIRTIO_MMIO_QUEUE_NUM          0x038
#define VIRTIO_MMIO_QUEUE_READY        0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY       0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS   0x060
#define VIRTIO_MMIO_INTERRUPT_ACK      0x064
#define VIRTIO_MMIO_STATUS             0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW     0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH    0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW    0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH   0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW     0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH    0x0a4

// VirtIO 状态位
#define VIRTIO_STATUS_ACKNOWLEDGE      1
#define VIRTIO_STATUS_DRIVER           2
#define VIRTIO_STATUS_FAILED           128
#define VIRTIO_STATUS_FEATURES_OK      8
#define VIRTIO_STATUS_DRIVER_OK        4
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET 64

// VirtIO 设备ID
#define VIRTIO_ID_BLOCK                2

// VirtIO 队列大小
#define VIRTIO_QUEUE_SIZE 8

// VirtIO 描述符标志
#define VIRTQ_DESC_F_NEXT   1
#define VIRTQ_DESC_F_WRITE  2
#define VIRTQ_DESC_F_INDIRECT 4

// VirtIO 块请求结构
struct virtio_blk_req {
    uint32 type;
    uint32 reserved;
    uint64 sector;
    uint8 data[512];
    uint8 status;
};

#define VIRTIO_BLK_T_IN           0
#define VIRTIO_BLK_T_OUT          1
#define VIRTIO_BLK_T_FLUSH        4
#define VIRTIO_BLK_T_FLUSH_OUT    5

#define VIRTIO_BLK_S_OK           0
#define VIRTIO_BLK_S_IOERR        1
#define VIRTIO_BLK_S_UNSUPP       2

// VirtIO 描述符
struct virtq_desc {
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
};

// VirtIO 可用环
struct virtq_avail {
    uint16 flags;
    uint16 idx;
    uint16 ring[VIRTIO_QUEUE_SIZE];
};

// VirtIO 已用环
struct virtq_used_elem {
    uint32 id;
    uint32 len;
};

struct virtq_used {
    uint16 flags;
    uint16 idx;
    struct virtq_used_elem ring[VIRTIO_QUEUE_SIZE];
};

// VirtIO 队列
struct virtq {
    uint16 num;
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;
};

// VirtIO 设备
struct virtio_dev {
    volatile uint32 *regs;
    struct virtq queues[1];  // 只使用队列0
    uint16 used_idx[1];
};

void virtio_init(void);
void virtio_intr(void);
int virtio_disk_rw(uint32 blockno, void *buf, int write);

#endif

