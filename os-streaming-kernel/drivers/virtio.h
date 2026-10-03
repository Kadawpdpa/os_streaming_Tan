#ifndef VIRTIO_H
#define VIRTIO_H
#include "kernel.h"

/* virtio-mmio: Supports both legacy (version 1) and modern (version 2) */
#define VIRTIO_MMIO_BASE   0x0a000000UL
#define VIRTIO_MMIO_STRIDE 0x200UL
#define VIRTIO_MMIO_SLOTS  32
#define VIRTIO_IRQ_BASE    48

#define VIRTIO_MAGIC       0x74726976
#define REG_MAGIC          0x000
#define REG_VERSION        0x004
#define REG_DEVICE_ID      0x008
#define REG_DEV_FEATURES   0x010
#define REG_DEV_FEAT_SEL   0x014
#define REG_DRV_FEATURES   0x020
#define REG_DRV_FEAT_SEL   0x024
#define REG_GUEST_PAGE     0x028
#define REG_QUEUE_SEL      0x030
#define REG_QUEUE_MAX      0x034
#define REG_QUEUE_NUM      0x038
#define REG_QUEUE_ALIGN    0x03c
#define REG_QUEUE_PFN      0x040
#define REG_QUEUE_READY    0x044
#define REG_QUEUE_NOTIFY   0x050
#define REG_INT_STATUS     0x060
#define REG_INT_ACK        0x064
#define REG_STATUS         0x070
#define REG_QUEUE_DESC_LO  0x080
#define REG_QUEUE_DESC_HI  0x084
#define REG_QUEUE_DRV_LO   0x090
#define REG_QUEUE_DRV_HI   0x094
#define REG_QUEUE_DEV_LO   0x0a0
#define REG_QUEUE_DEV_HI   0x0a4
#define REG_CONFIG         0x100

#define ST_ACK     1
#define ST_DRIVER  2
#define ST_DRV_OK  4
#define ST_FEAT_OK 8
#define ST_FAILED  0x80

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

struct vring_desc  { u64 addr; u32 len; u16 flags; u16 next; };
struct vring_avail { u16 flags; u16 idx; u16 ring[]; };
struct vring_used_elem { u32 id; u32 len; };
struct vring_used  { u16 flags; u16 idx; struct vring_used_elem ring[]; };

struct virtq {
    u16 num;
    struct vring_desc  *desc;
    struct vring_avail *avail;
    struct vring_used  *used;
    u16 last_used;
};

struct virtio_dev {
    u64 base;
    unsigned slot;
    u32 version;
};

int  virtio_find(u32 device_id, struct virtio_dev *d);
int  virtio_begin(struct virtio_dev *d, u64 want_features);      /* reset, ACK, DRIVER, negotiate features */
int  virtio_queue_setup(struct virtio_dev *d, unsigned idx, struct virtq *q, unsigned want);
void virtio_ready(struct virtio_dev *d);                          /* FEATURES_OK + DRIVER_OK */
void virtq_push(struct virtio_dev *d, unsigned qidx, struct virtq *q, u16 head);

#endif