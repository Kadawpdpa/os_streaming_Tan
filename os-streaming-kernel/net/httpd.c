/* HTTP/1.1 server using lwIP raw API: GET/HEAD + Range (allows browsers to seek video) */
#include "kernel.h"
#include <string.h>
#include <ctype.h>
#include "lwip/tcp.h"

#define CHUNK 8192
static u8 iobuf[CHUNK];                                /* Can be shared because all callbacks run in a single net thread */

struct hs {
    struct tcp_pcb *pcb;
    char req[1024];
    unsigned req_len;
    int started;
    const struct tar_file *file;
    char *mem;                                         /* In-memory generated body (status page, 404) */
    u64 pos, remaining;
    int polls;
};

static const char *mime(const char *name) {
    const char *ext = 0, *p = name;
    while ((p = strchr(p, '.')) != 0) { ext = p; p++; }
    if (!ext) return "application/octet-stream";
    if (!strcmp(ext, ".html") || !strcmp(ext, ".htm")) return "text/html; charset=utf-8";
    if (!strcmp(ext, ".css"))  return "text/css";
    if (!strcmp(ext, ".js"))   return "text/javascript";
    if (!strcmp(ext, ".json")) return "application/json";
    if (!strcmp(ext, ".txt") || !strcmp(ext, ".md")) return "text/plain; charset=utf-8";
    if (!strcmp(ext, ".mp4") || !strcmp(ext, ".m4v")) return "video/mp4";
    if (!strcmp(ext, ".webm")) return "video/webm";
    if (!strcmp(ext, ".mkv"))  return "video/x-matroska";
    if (!strcmp(ext, ".m3u8")) return "application/vnd.apple.mpegurl";
    if (!strcmp(ext, ".ts"))   return "video/mp2t";
    if (!strcmp(ext, ".m4s"))  return "video/iso.segment";
    if (!strcmp(ext, ".mp3"))  return "audio/mpeg";
    if (!strcmp(ext, ".aac") || !strcmp(ext, ".m4a")) return "audio/mp4";
    if (!strcmp(ext, ".jpg") || !strcmp(ext, ".jpeg")) return "image/jpeg";
    if (!strcmp(ext, ".png"))  return "image/png";
    if (!strcmp(ext, ".svg"))  return "image/svg+xml";
    if (!strcmp(ext, ".ico"))  return "image/x-icon";
    return "application/octet-stream";
}

static void hs_free(struct hs *hs) {
    if (hs->mem) kfree(hs->mem);
    kfree(hs);
}

static void hs_close(struct hs *hs, struct tcp_pcb *pcb) {
    tcp_arg(pcb, 0); tcp_recv(pcb, 0); tcp_sent(pcb, 0); tcp_poll(pcb, 0, 0); tcp_err(pcb, 0);
    if (tcp_close(pcb) != ERR_OK) tcp_abort(pcb);
    hs_free(hs);
}

/* Send data as much as the TCP send window allows */
static void send_more(struct hs *hs, struct tcp_pcb *pcb) {
    while (hs->remaining > 0) {
        u16_t space = tcp_sndbuf(pcb);
        if (space < 256) break;
        size_t chunk = hs->remaining < CHUNK ? (size_t)hs->remaining : CHUNK;
        if (chunk > space) chunk = space;
        if (hs->file) {
            int r = tarfs_read(hs->file, hs->pos, iobuf, chunk);
            if (r <= 0) { hs_close(hs, pcb); return; }
            chunk = (size_t)r;
        } else {
            memcpy(iobuf, hs->mem + hs->pos, chunk);
        }
        u8_t flags = TCP_WRITE_FLAG_COPY | (hs->remaining > chunk ? TCP_WRITE_FLAG_MORE : 0);
        err_t e = tcp_write(pcb, iobuf, (u16_t)chunk, flags);
        if (e == ERR_MEM) break;                       /* Queue full: wait for the sent callback */
        if (e != ERR_OK) { hs_close(hs, pcb); return; }
        hs->pos += chunk; hs->remaining -= chunk;
    }
    tcp_output(pcb);
    if (hs->remaining == 0) hs_close(hs, pcb);         /* Data is queued for sending; lwIP will transmit it and close the connection */
}

static void respond(struct hs *hs, struct tcp_pcb *pcb, int code, const char *ctype,
                    u64 total, u64 start, u64 len, int partial, int head_only) {
    static const char *msg_ok = "OK", *msg_pc = "Partial Content", *msg_nf = "Not Found", *msg_rng = "Range Not Satisfiable";
    const char *msg = code == 200 ? msg_ok : code == 206 ? msg_pc : code == 404 ? msg_nf : msg_rng;
    char h[384];
    int n = ksnprintf(h, sizeof(h), "HTTP/1.1 %d %s\r\nServer: OS-Streaming-Kernel\r\nContent-Type: %s\r\n"
                      "Accept-Ranges: bytes\r\nConnection: close\r\nContent-Length: %lu\r\n", code, msg, ctype, (unsigned long)len);
    if (partial)
        n += ksnprintf(h + n, sizeof(h) - n, "Content-Range: bytes %lu-%lu/%lu\r\n",
                       (unsigned long)start, (unsigned long)(start + len - 1), (unsigned long)total);
    if (code == 416)
        n += ksnprintf(h + n, sizeof(h) - n, "Content-Range: bytes */%lu\r\n", (unsigned long)total);
    n += ksnprintf(h + n, sizeof(h) - n, "\r\n");
    tcp_write(pcb, h, (u16_t)n, TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE);
    hs->pos = start;
    hs->remaining = head_only ? 0 : len;
    send_more(hs, pcb);
}

static void make_status(struct hs *hs) {
    hs->mem = kmalloc(1024);
    if (!hs->mem) return;
    char ip[32]; net_get_ip(ip, sizeof(ip));
    int n = ksnprintf(hs->mem, 1024,
        "OS Streaming Kernel (AArch64)\n"
        "uptime        : %lu ms\n"
        "ip            : %s\n"
        "pages free    : %lu / %lu (4 KB each)\n"
        "kernel heap   : %lu bytes in use\n"
        "files on disk : %d\n"
        "scheduler     : preemptive round-robin, 10 ms slice (%lu ticks)\n"
        "disk sectors  : %lu\n",
        (unsigned long)uptime_ms(), ip, (unsigned long)pmm_free_pages(), (unsigned long)pmm_total_pages(),
        (unsigned long)kheap_used(), tarfs_count(), (unsigned long)timer_ticks(), (unsigned long)blk_sectors());
    hs->remaining = (u64)n;
}

static void handle_request(struct hs *hs, struct tcp_pcb *pcb) {
    char *line_end = strstr(hs->req, "\r\n");
    if (!line_end) { hs_close(hs, pcb); return; }
    *line_end = 0;
    int head_only = !strncmp(hs->req, "HEAD ", 5);
    if (strncmp(hs->req, "GET ", 4) && !head_only) { hs_close(hs, pcb); return; }
    char *path = hs->req + (head_only ? 5 : 4);
    char *sp = strchr(path, ' ');
    if (sp) *sp = 0;
    char *q = strchr(path, '?');
    if (q) *q = 0;
    if (!strcmp(path, "/")) path = "/index.html";

    u64 rs = 0, re = 0; int has_range = 0, open_end = 0;
    char *hdrs = line_end + 2;
    for (char *p = hdrs; *p; p++) {
        if (strncasecmp(p, "range: bytes=", 13) == 0) {
            p += 13;
            has_range = 1;
            if (*p == '-') {                                    /* suffix: bytes=-N */
                u64 nlast = 0; p++;
                while (isdigit(*p)) nlast = nlast * 10 + (*p++ - '0');
                rs = nlast; open_end = 2;               /* bytes=-N: Last N bytes (calculated when file size is known) */
            } else {
                while (isdigit(*p)) rs = rs * 10 + (*p++ - '0');
                if (*p == '-') p++;
                if (isdigit(*p)) { while (isdigit(*p)) re = re * 10 + (*p++ - '0'); }
                else open_end = 1;
            }
            break;
        }
        char *nl = strstr(p, "\r\n");
        if (!nl) break;
        p = nl + 1;
    }

    if (!strcmp(path, "/status")) {
        make_status(hs);
        if (!hs->mem) { hs_close(hs, pcb); return; }
        respond(hs, pcb, 200, "text/plain; charset=utf-8", hs->remaining, 0, hs->remaining, 0, head_only);
        return;
    }
    const struct tar_file *f = tarfs_find(path);
    if (!f) {
        static const char body[] = "404 Not Found\n";
        hs->mem = kmalloc(sizeof(body));
        if (hs->mem) memcpy(hs->mem, body, sizeof(body));
        respond(hs, pcb, 404, "text/plain", sizeof(body) - 1, 0, sizeof(body) - 1, 0, head_only);
        return;
    }
    hs->file = f;
    if (!has_range) { respond(hs, pcb, 200, mime(f->name), f->size, 0, f->size, 0, head_only); return; }
    if (open_end == 2) { u64 nlast = rs; rs = nlast > f->size ? 0 : f->size - nlast; re = f->size - 1; }
    else if (open_end == 1 || re >= f->size) re = f->size - 1;
    if (rs >= f->size || rs > re) { respond(hs, pcb, 416, "text/plain", f->size, 0, 0, 0, 1); return; }
    respond(hs, pcb, 206, mime(f->name), f->size, rs, re - rs + 1, 1, head_only);
}

static err_t cb_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    struct hs *hs = arg;
    if (!p) { if (hs) hs_close(hs, pcb); return ERR_OK; }                 /* Remote host closed the connection */
    if (err != ERR_OK || !hs) { pbuf_free(p); return ERR_OK; }
    tcp_recved(pcb, p->tot_len);
    if (!hs->started) {
        unsigned room = sizeof(hs->req) - 1 - hs->req_len;
        unsigned n = p->tot_len < room ? p->tot_len : room;
        pbuf_copy_partial(p, hs->req + hs->req_len, (u16_t)n, 0);
        hs->req_len += n; hs->req[hs->req_len] = 0;
        if (strstr(hs->req, "\r\n\r\n")) { hs->started = 1; pbuf_free(p); handle_request(hs, pcb); return ERR_OK; }
        if (hs->req_len >= sizeof(hs->req) - 1) { pbuf_free(p); hs_close(hs, pcb); return ERR_OK; }
    }
    pbuf_free(p);
    return ERR_OK;
}

static err_t cb_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    (void)len;
    struct hs *hs = arg;
    if (hs && hs->started) send_more(hs, pcb);
    return ERR_OK;
}

static err_t cb_poll(void *arg, struct tcp_pcb *pcb) {
    struct hs *hs = arg;
    if (!hs) return ERR_OK;
    if (++hs->polls > 30) { hs_close(hs, pcb); return ERR_ABRT; }        /* ~60 seconds with no progress */
    if (hs->started) send_more(hs, pcb);
    return ERR_OK;
}

static void cb_err(void *arg, err_t err) { (void)err; if (arg) hs_free(arg); }

static err_t cb_accept(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)arg;
    if (err != ERR_OK) return err;
    struct hs *hs = kmalloc(sizeof(*hs));
    if (!hs) return ERR_MEM;
    memset(hs, 0, sizeof(*hs));
    hs->pcb = pcb;
    tcp_setprio(pcb, TCP_PRIO_MIN);
    tcp_arg(pcb, hs);
    tcp_recv(pcb, cb_recv);
    tcp_sent(pcb, cb_sent);
    tcp_err(pcb, cb_err);
    tcp_poll(pcb, cb_poll, 4);
    return ERR_OK;
}

void httpd_init(void) {
    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ANY_TYPE, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, cb_accept);
}