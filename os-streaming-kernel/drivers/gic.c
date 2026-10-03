/* GICv2 (QEMU virt: distributor 0x08000000, CPU interface 0x08010000) */
#include "kernel.h"
#define GICD 0x08000000UL
#define GICC 0x08010000UL

void gic_init(void) {
    wr32(GICD + 0x000, 1);          /* GICD_CTLR */
    wr32(GICC + 0x004, 0xff);       /* GICC_PMR: Receive all priority */
    wr32(GICC + 0x000, 1);          /* GICC_CTLR */
}
void gic_enable_irq(unsigned irq) {
    if (irq >= 32) *(volatile u8 *)(GICD + 0x800 + irq) = 1;     /* ITARGETSR: Send to CPU0 */
    wr32(GICD + 0x100 + 4 * (irq / 32), 1u << (irq % 32));       /* ISENABLER */
}
u32  gic_ack(void)     { return rd32(GICC + 0x00C); }
void gic_eoi(u32 iar)  { wr32(GICC + 0x010, iar); }
