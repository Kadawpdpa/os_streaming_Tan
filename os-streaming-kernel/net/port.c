/* Functions required by lwIP from the platform */
#include "kernel.h"

u32 sys_now(void) { return (u32)uptime_ms(); }

static u64 rng;
unsigned int lwip_rand32(void) {            /* xorshift64, seeded from CPU counter */
    if (!rng) { u64 c; __asm__ volatile("mrs %0, cntpct_el0" : "=r"(c)); rng = c | 1; }
    rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
    return (unsigned int)(rng >> 16);
}

int atoi(const char *s) {
    int n = 0, neg = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return neg ? -n : n;
}