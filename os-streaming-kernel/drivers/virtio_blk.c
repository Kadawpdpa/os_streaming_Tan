/* virtio-blk: Read sectors like synchronous (polling) */
#include "virtio.h"
#include <string.h>

struct blk_req_hdr { u32 type; u32 reserved; u64 sector; };

static struct virtio_dev dev;
static struct virtq q;
static u64 capacity;
static int ready;
static struct blk_req_hdr *hdr;
static volatile u8 *status_byte;

int blk_init(void) {
    if (virtio_find(2, &dev) < 0) { kprintf("[blk]  ไม่พบ virtio-blk (ไม่ได้ต่อดิสก์)\n"); return -1; }
    virtio_begin(&dev, 0);
    if (virtio_queue_setup(&dev, 0, &q, 16) < 0) return -1;
    virtio_ready(&dev);
    capacity = rd32(dev.base + REG_CONFIG) | ((u64)rd32(dev.base + REG_CONFIG + 4) << 32);
    u8 *p = pmm_alloc(1);
    hdr = (struct blk_req_hdr *)p;
    status_byte = p + 64;
    ready = 1;
    kprintf("[blk]  virtio-blk slot %u (v%u): %lu sectors (%lu MB)\n", dev.slot, dev.version,
            (unsigned long)capacity, (unsigned long)(capacity >> 11));
    return 0;
}
u64 blk_sectors(void) { return ready ? capacity : 0; }

int blk_read(u64 sector, unsigned count, void *buf) {
    if (!ready || sector + count > capacity) return -1;
    unsigned long f = irq_save();
    hdr->type = 0; hdr->reserved = 0; hdr->sector = sector;     /* VIRTIO_BLK_T_IN */
    *status_byte = 0xff;
    q.desc[0] = (struct vring_desc){ (u64)hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1 };
    q.desc[1] = (struct vring_desc){ (u64)buf, count * 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2 };
    q.desc[2] = (struct vring_desc){ (u64)status_byte, 1, VRING_DESC_F_WRITE, 0 };
    virtq_push(&dev, 0, &q, 0);
    while (q.used->idx == q.last_used) { dmb(); }
    q.last_used = q.used->idx;
    dmb();
    int r = *status_byte == 0 ? 0 : -2;
    irq_restore(f);
    return r;
}
