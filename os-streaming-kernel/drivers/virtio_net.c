/* virtio-net: RX/TX via polling + IRQ to wake up from wfi */
#include "virtio.h"
#include <string.h>

#define BUF_SIZE 2048
#define RX_BUFS  32

static struct virtio_dev dev;
static struct virtq rxq, txq;
static u8 *rx_mem, *tx_mem;
static unsigned hdr_len;
static int ready;

int virtio_net_init(u8 mac[6]) {
    if (virtio_find(1, &dev) < 0) { kprintf("[net]  virtio-net not found\n"); return -1; }
    virtio_begin(&dev, 1u << 5);                         // VIRTIO_NET_F_MAC
    hdr_len = dev.version >= 2 ? 12 : 10;
    if (virtio_queue_setup(&dev, 0, &rxq, RX_BUFS) < 0) return -1;
    if (virtio_queue_setup(&dev, 1, &txq, 8) < 0) return -1;
    rx_mem = pmm_alloc(RX_BUFS * BUF_SIZE / PAGE_SIZE);
    tx_mem = pmm_alloc(BUF_SIZE / PAGE_SIZE + 1);
    if (!rx_mem || !tx_mem) return -2;
    for (unsigned i = 0; i < rxq.num; i++) {
        rxq.desc[i] = (struct vring_desc){ (u64)(rx_mem + i * BUF_SIZE), BUF_SIZE, VRING_DESC_F_WRITE, 0 };
        rxq.avail->ring[i] = i;
    }
    dmb(); rxq.avail->idx = rxq.num; dmb();
    virtio_ready(&dev);
    wr32(dev.base + REG_QUEUE_NOTIFY, 0);
    for (int i = 0; i < 6; i++) mac[i] = rd8(dev.base + REG_CONFIG + i);
    gic_enable_irq(VIRTIO_IRQ_BASE + dev.slot);          // Allow incoming packets to wake the CPU from wfi
    ready = 1;
    kprintf("[net]  virtio-net slot %u (v%u) MAC %02x:%02x:%02x:%02x:%02x:%02x\n", dev.slot, dev.version,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}

int virtio_net_has_rx(void) { dmb(); return ready && rxq.used->idx != rxq.last_used; }

/* Pass all received frames to the callback and return the frame count */
int virtio_net_poll(void (*cb)(const u8 *frame, unsigned len)) {
    int n = 0;
    while (virtio_net_has_rx()) {
        struct vring_used_elem e = rxq.used->ring[rxq.last_used % rxq.num];
        dmb();
        const u8 *buf = rx_mem + e.id * BUF_SIZE;
        if (e.len > hdr_len) cb(buf + hdr_len, e.len - hdr_len);
        rxq.avail->ring[rxq.avail->idx % rxq.num] = (u16)e.id;     // Return the buffer to the device
        dmb(); rxq.avail->idx++; rxq.last_used++;
        n++;
    }
    if (n) { dmb(); wr32(dev.base + REG_QUEUE_NOTIFY, 0); }
    return n;
}

int virtio_net_send(const u8 *frame, unsigned len) {
    if (!ready || len + hdr_len > BUF_SIZE) return -1;
    unsigned long f = irq_save();
    memset(tx_mem, 0, hdr_len);
    memcpy(tx_mem + hdr_len, frame, len);
    txq.desc[0] = (struct vring_desc){ (u64)tx_mem, len + hdr_len, 0, 0 };
    u16 before = txq.used->idx;
    virtq_push(&dev, 1, &txq, 0);
    while (txq.used->idx == before) dmb();                 // Wait for the device to transmit (very fast on QEMU)
    irq_restore(f);
    return 0;
}