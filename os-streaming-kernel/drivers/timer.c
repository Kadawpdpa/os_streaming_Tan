/* ARM generic timer (physical, PPI 30) */
#include "kernel.h"

static u64 interval, freq, ticks;

static inline u64 cntpct(void) { u64 v; __asm__ volatile("isb\n mrs %0, cntpct_el0" : "=r"(v)); return v; }

void timer_init(unsigned hz) {
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    interval = freq / hz;
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(interval));
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(1UL));
    gic_enable_irq(30);
}
void timer_rearm(void) {
    ticks++;
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(interval));
}
u64 timer_ticks(void) { return ticks; }
u64 uptime_ms(void)   { return cntpct() / (freq / 1000); }
