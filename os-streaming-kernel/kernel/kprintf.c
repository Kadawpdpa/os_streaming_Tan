/* Small printf: %d %i %u %x %X %p %s %c %% with 0, width, l / ll / z */
#include "kernel.h"
#include <string.h>

struct out { char *buf; size_t pos, cap; int uart; };

static void emit(struct out *o, char c) {
    if (o->uart) { if (c == '\n') uart_putc('\r'); uart_putc(c); return; }
    if (o->pos + 1 < o->cap) o->buf[o->pos] = c;
    o->pos++;
}

static void emit_num(struct out *o, u64 v, int base, int upper, int neg, int width, int zero, int left) {
    char tmp[24]; int n = 0;
    const char *dg = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (!v) tmp[n++] = '0';
    while (v) { tmp[n++] = dg[v % base]; v /= base; }
    int len = n + (neg ? 1 : 0), pad = width > len ? width - len : 0;
    if (neg && zero) emit(o, '-');
    if (!left) for (int k = 0; k < pad; k++) emit(o, zero ? '0' : ' ');
    if (neg && !zero) emit(o, '-');
    while (n) emit(o, tmp[--n]);
    if (left) for (int k = 0; k < pad; k++) emit(o, ' ');
}

static void format(struct out *o, const char *f, va_list ap) {
    for (; *f; f++) {
        if (*f != '%') { emit(o, *f); continue; }
        f++;
        int zero = 0, width = 0, lng = 0, left = 0;
        if (*f == '-') { left = 1; f++; }
        if (*f == '0') { zero = 1; f++; }
        while (*f >= '0' && *f <= '9') { width = width * 10 + (*f - '0'); f++; }
        while (*f == 'l' || *f == 'z') { lng++; f++; }
        switch (*f) {
        case 'd': case 'i': {
            long long v = lng ? va_arg(ap, long) : va_arg(ap, int);
            int neg = v < 0; emit_num(o, neg ? -v : v, 10, 0, neg, width, zero, left); break; }
        case 'u': emit_num(o, lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 10, 0, 0, width, zero, left); break;
        case 'x': emit_num(o, lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 16, 0, 0, width, zero, left); break;
        case 'X': emit_num(o, lng ? va_arg(ap, unsigned long) : va_arg(ap, unsigned), 16, 1, 0, width, zero, left); break;
        case 'p': emit(o, '0'); emit(o, 'x'); emit_num(o, (u64)va_arg(ap, void *), 16, 0, 0, 0, 0, 0); break;
        case 'c': emit(o, (char)va_arg(ap, int)); break;
        case 's': { const char *s = va_arg(ap, const char *); if (!s) s = "(null)";
            int len = (int)strlen(s), pad = width > len ? width - len : 0;
            if (!left) for (int k = 0; k < pad; k++) emit(o, ' ');
            while (*s) emit(o, *s++);
            if (left) for (int k = 0; k < pad; k++) emit(o, ' ');
            break; }
        case '%': emit(o, '%'); break;
        case 0: return;
        default: emit(o, '%'); emit(o, *f); break;
        }
    }
}

int kvsnprintf(char *buf, size_t n, const char *fmt, va_list ap) {
    struct out o = { buf, 0, n, 0 };
    format(&o, fmt, ap);
    if (n) buf[o.pos < n ? o.pos : n - 1] = 0;
    return (int)o.pos;
}
int ksnprintf(char *buf, size_t n, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int r = kvsnprintf(buf, n, fmt, ap);
    va_end(ap); return r;
}
void kprintf(const char *fmt, ...) {
    struct out o = { 0, 0, 0, 1 };
    va_list ap; va_start(ap, fmt);
    unsigned long f = irq_save();          /* Prevent messages from multiple threads from interleaving */
    format(&o, fmt, ap);
    irq_restore(f);
    va_end(ap);
}
void panic(const char *msg) {
    irq_save();
    kprintf("\n*** KERNEL PANIC: %s\n", msg);
    for (;;) __asm__ volatile("wfe");
}