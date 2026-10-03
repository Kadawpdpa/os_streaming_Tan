#include "thread.h"

#define UART0 ((volatile unsigned int *)0x09000000)

#define GICD_CTLR       (*(volatile unsigned int *)0x08000000)
#define GICD_ISENABLER0 (*(volatile unsigned int *)0x08000100)
#define GICC_CTLR       (*(volatile unsigned int *)0x08010000)
#define GICC_PMR        (*(volatile unsigned int *)0x08010004)
#define GICC_IAR        (*(volatile unsigned int *)0x0801000C)
#define GICC_EOIR       (*(volatile unsigned int *)0x08010010)
#define TIMER_IRQ       30
#define TICKS_PER_SEC   100

extern char vectors[];
static unsigned long tick_interval;
static unsigned long ticks;

static void putc(char c) { *UART0 = c; }

static void puts(const char *s) {
    unsigned long f = irq_save();
    while (*s) {
        if (*s == '\n') putc('\r');
        putc(*s++);
    }
    irq_restore(f);
}

static void print_dec(unsigned long n) {
    char b[21]; int i = 0;
    if (!n) { putc('0'); return; }
    while (n) { b[i++] = '0' + n % 10; n /= 10; }
    while (i) putc(b[--i]);
}

static void print_hex(unsigned long n) {
    puts("0x");
    for (int s = 60; s >= 0; s -= 4) putc("0123456789abcdef"[(n >> s) & 0xf]);
}

static unsigned long read_cntfrq(void) {
    unsigned long v;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v));
    return v;
}
static void timer_set(unsigned long t) { __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(t)); }
static void timer_enable(void)         { __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(1UL)); }

static void gic_init(void) {
    GICD_CTLR = 1;
    GICD_ISENABLER0 = 1u << TIMER_IRQ;
    GICC_PMR = 0xff;
    GICC_CTLR = 1;
}

void handle_irq(void) {
    unsigned int iar = GICC_IAR;
    unsigned int id = iar & 0x3ff;
    int preempt = 0;
    if (id == TIMER_IRQ) {
        timer_set(tick_interval);
        ticks++;
        if (ticks % TICKS_PER_SEC == 0) {
            puts("[timer] "); print_dec(ticks / TICKS_PER_SEC); puts(" s\n");
        }
        preempt = 1;
    }
    GICC_EOIR = iar;
    if (preempt) yield();
}

void handle_unexpected(void) {
    unsigned long esr;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    puts("Unexpected exception! ESR_EL1 = "); print_hex(esr); puts("\n");
    for (;;) __asm__ volatile("wfe");
}

static void worker(void) {
    int id = thread_self();
    for (int round = 1; round <= 5; round++) {
        for (volatile unsigned long i = 0; i < 4000000; i++) { }
        unsigned long f = irq_save();
        puts("  thread "); print_dec(id);
        puts(" finished round "); print_dec(round);
        puts(" (at tick "); print_dec(ticks); puts(")\n");
        irq_restore(f);
    }
}

void kmain(void) {
    puts("Hello from my own kernel on AArch64!\n");

    __asm__ volatile("msr vbar_el1, %0\n isb" :: "r"(vectors));
    gic_init();
    tick_interval = read_cntfrq() / TICKS_PER_SEC;
    timer_set(tick_interval);
    timer_enable();

    thread_create(worker);
    thread_create(worker);
    thread_create(worker);

    puts("main: starting threads (preemptive)\n");
    __asm__ volatile("msr daifclr, #2");
    while (threads_running() > 1) __asm__ volatile("wfi");
    puts("main: all threads finished\n");

    for (;;) __asm__ volatile("wfi");
}
