/* Connect lwIP with virtio-net: netif + thread polling for packets */
#include "kernel.h"
#include <string.h>
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "lwip/etharp.h"
#include "lwip/pbuf.h"
#include "lwip/ip4_addr.h"
#include "netif/ethernet.h"

int  virtio_net_init(u8 mac[6]);
int  virtio_net_poll(void (*cb)(const u8 *frame, unsigned len));
int  virtio_net_has_rx(void);
int  virtio_net_send(const u8 *frame, unsigned len);

/* Static IP for QEMU simulated network (user-mode networking):
 * Our machine 10.0.2.15, gateway 10.0.2.2. If using on a real network, modify this (or enable LWIP_DHCP) */
#define OS_IP   "10.0.2.15"
#define OS_MASK "255.255.255.0"
#define OS_GW   "10.0.2.2"

static struct netif nif;
static int net_ok;

static err_t low_output(struct netif *n, struct pbuf *p) {
    (void)n;
    static u8 frame[1600];
    if (p->tot_len > sizeof(frame)) return ERR_BUF;
    pbuf_copy_partial(p, frame, p->tot_len, 0);
    return virtio_net_send(frame, p->tot_len) == 0 ? ERR_OK : ERR_IF;
}

static err_t netif_setup(struct netif *n) {
    n->name[0] = 'v'; n->name[1] = 'n';
    n->output = etharp_output;
    n->linkoutput = low_output;
    n->mtu = 1500;
    n->hwaddr_len = 6;
    n->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
    return ERR_OK;
}

static void rx_frame(const u8 *frame, unsigned len) {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);
    if (!p) return;                                    /* Pool full: drop packet (TCP will retransmit automatically) */
    pbuf_take(p, frame, (u16_t)len);
    if (nif.input(p, &nif) != ERR_OK) pbuf_free(p);
}

int net_init(void) {
    u8 mac[6];
    if (virtio_net_init(mac) < 0) return -1;
    lwip_init();
    ip4_addr_t ip, mask, gw;
    ip4addr_aton(OS_IP, &ip); ip4addr_aton(OS_MASK, &mask); ip4addr_aton(OS_GW, &gw);
    memcpy(nif.hwaddr, mac, 6);
    netif_add(&nif, &ip, &mask, &gw, 0, netif_setup, ethernet_input);
    memcpy(nif.hwaddr, mac, 6);
    netif_set_default(&nif);
    netif_set_up(&nif);
    httpd_init();
    net_ok = 1;
    kprintf("[net]  lwIP is ready IP %s, HTTP server at port 80\n", OS_IP);
    return 0;
}

void net_get_ip(char *buf, size_t n) {
    if (!net_ok) { ksnprintf(buf, n, "(no network)"); return; }
    ksnprintf(buf, n, "%s", ip4addr_ntoa(netif_ip4_addr(&nif)));
}

void net_thread(void *arg) {
    (void)arg;
    if (!net_ok) return;
    for (;;) {
        int n = virtio_net_poll(rx_frame);
        sys_check_timeouts();                          /* lwIP timers (TCP retransmit, ARP) */
        if (!n) {
            unsigned long f = irq_save();              /* Disable IRQs before checking to avoid missing wakeup signals */
            if (!virtio_net_has_rx()) __asm__ volatile("wfi");
            irq_restore(f);
        }
    }
}