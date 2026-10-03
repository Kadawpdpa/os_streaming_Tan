#include "kernel.h"

#define TIMER_IRQ    30
#define VIRTIO_IRQ0  48                    /* virtio-mmio slot n = SPI 48+n */

void handle_irq(void) {
    u32 iar = gic_ack();
    u32 id = iar & 0x3ff;
    if (id >= 1020) return;                /* spurious: no EOI needed */
    int preempt = 0;
    if (id == TIMER_IRQ) {
        timer_rearm();
        thread_tick();
        preempt = 1;
    } else if (id >= VIRTIO_IRQ0 && id < VIRTIO_IRQ0 + 32) {
        virtio_irq(id - VIRTIO_IRQ0);
    }
    gic_eoi(iar);                          /* Must EOI before context switching */
    if (preempt) yield();                  /* Time slice exhausted -> preempt CPU */
}

void handle_user_sync(struct trapframe *tf) {
    u64 esr; __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    if ((esr >> 26) == 0x15) user_syscall(tf);     /* EC 0x15 = SVC (AArch64) */
    else user_fault(tf, esr);
}

void handle_fatal(struct trapframe *tf, int kind) {
    u64 esr, far;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(esr));
    __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    kprintf("\n*** KERNEL EXCEPTION (%s)\n", kind ? "sync, EL1" : "unexpected vector");
    kprintf("    ESR=0x%lx (EC=0x%lx)  ELR=0x%lx  FAR=0x%lx  SPSR=0x%lx\n",
            (unsigned long)esr, (unsigned long)(esr >> 26), (unsigned long)tf->elr,
            (unsigned long)far, (unsigned long)tf->spsr);
    for (;;) __asm__ volatile("wfe");
}