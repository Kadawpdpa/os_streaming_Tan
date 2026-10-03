#include "virtio.h"
#include <string.h>


int virtio_find(u32 device_id, struct virtio_dev *d) {
    for (unsigned s = 0; s < VIRTIO_MMIO_SLOTS; s++) {
        u64 base = VIRTIO_MMIO_BASE + s * VIRTIO_MMIO_STRIDE;
        if (rd32(base + REG_MAGIC) != VIRTIO_MAGIC) continue;
        if (rd32(base + REG_DEVICE_ID) != device_id) continue;
        d->base = base; d->slot = s; d->version = rd32(base + REG_VERSION);
        return 0;
    }
    return -1;
}

int virtio_begin(struct virtio_dev *d, u64 want) {
    wr32(d->base + REG_STATUS, 0);                       /* reset */
    wr32(d->base + REG_STATUS, ST_ACK);
    wr32(d->base + REG_STATUS, ST_ACK | ST_DRIVER);
    u64 have = 0;
    wr32(d->base + REG_DEV_FEAT_SEL, 0); have |= rd32(d->base + REG_DEV_FEATURES);
    if (d->version >= 2) { wr32(d->base + REG_DEV_FEAT_SEL, 1); have |= (u64)rd32(d->base + REG_DEV_FEATURES) << 32; }
    u64 use = have & want;
    if (d->version >= 2) use |= (1ULL << 32);            /* VIRTIO_F_VERSION_1 */
    wr32(d->base + REG_DRV_FEAT_SEL, 0); wr32(d->base + REG_DRV_FEATURES, (u32)use);
    if (d->version >= 2) { wr32(d->base + REG_DRV_FEAT_SEL, 1); wr32(d->base + REG_DRV_FEATURES, (u32)(use >> 32)); }
    if (d->version < 2) wr32(d->base + REG_GUEST_PAGE, PAGE_SIZE);
    return 0;
}

/* Legacy virtqueue layout: desc | avail | (padding) | used — can also be used with modern */
int virtio_queue_setup(struct virtio_dev *d, unsigned idx, struct virtq *q, unsigned want) {
    wr32(d->base + REG_QUEUE_SEL, idx);
    u32 max = rd32(d->base + REG_QUEUE_MAX);
    if (!max) return -1;
    unsigned num = want < max ? want : max;
    size_t desc_sz  = 16 * num;
    size_t avail_sz = 6 + 2 * num;
    size_t used_off = (desc_sz + avail_sz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t used_sz  = 6 + 8 * num;
    size_t total    = (used_off + used_sz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    u8 *mem = pmm_alloc(total / PAGE_SIZE);
    if (!mem) return -2;
    q->num = num;
    q->desc  = (struct vring_desc *)mem;
    q->avail = (struct vring_avail *)(mem + desc_sz);
    q->used  = (struct vring_used *)(mem + used_off);
    q->last_used = 0;
    wr32(d->base + REG_QUEUE_NUM, num);
    if (d->version < 2) {
        wr32(d->base + REG_QUEUE_ALIGN, PAGE_SIZE);
        wr32(d->base + REG_QUEUE_PFN, (u32)((u64)mem >> 12));
    } else {
        u64 a;
        a = (u64)q->desc;  wr32(d->base + REG_QUEUE_DESC_LO, (u32)a); wr32(d->base + REG_QUEUE_DESC_HI, (u32)(a >> 32));
        a = (u64)q->avail; wr32(d->base + REG_QUEUE_DRV_LO,  (u32)a); wr32(d->base + REG_QUEUE_DRV_HI,  (u32)(a >> 32));
        a = (u64)q->used;  wr32(d->base + REG_QUEUE_DEV_LO,  (u32)a); wr32(d->base + REG_QUEUE_DEV_HI,  (u32)(a >> 32));
        wr32(d->base + REG_QUEUE_READY, 1);
    }
    return 0;
}

void virtio_ready(struct virtio_dev *d) {
    if (d->version >= 2) {
        wr32(d->base + REG_STATUS, ST_ACK | ST_DRIVER | ST_FEAT_OK);
        if (!(rd32(d->base + REG_STATUS) & ST_FEAT_OK)) kprintf("[virtio] feature negotiation failed\n");
    }
    wr32(d->base + REG_STATUS, rd32(d->base + REG_STATUS) | ST_DRV_OK);
}

void virtq_push(struct virtio_dev *d, unsigned qidx, struct virtq *q, u16 head) {
    q->avail->ring[q->avail->idx % q->num] = head;
    dmb();
    q->avail->idx++;
    dmb();
    wr32(d->base + REG_QUEUE_NOTIFY, qidx);
}

/* ---- IRQ: Just ACK so the device stops asserting the signal, then wake up threads waiting with wfi ---- */
void virtio_irq(unsigned slot) {
    if (slot >= VIRTIO_MMIO_SLOTS) return;
    u64 base = VIRTIO_MMIO_BASE + slot * VIRTIO_MMIO_STRIDE;
    u32 st = rd32(base + REG_INT_STATUS);
    if (st) wr32(base + REG_INT_ACK, st);
}