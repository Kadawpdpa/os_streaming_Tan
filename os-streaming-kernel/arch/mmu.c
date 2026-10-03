#include "kernel.h"
#include <string.h>

#define PTE_VALID   (1UL << 0)
#define PTE_TABLE   (3UL << 0)
#define PTE_BLOCK   (1UL << 0)
#define PTE_ATTR(i) ((unsigned long)(i) << 2)
#define PTE_AP_USER (1UL << 6) //AP[1]: EL0 Can access
#define PTE_SH_INNER (3UL << 8)
#define PTE_AF      (1UL << 10)
#define PTE_PXN     (1UL << 53)
#define PTE_UXN     (1UL << 54)

#define ATTR_DEVICE 0
#define ATTR_NORMAL 1

static u64 l1[512]  __attribute__((aligned(4096)));
static u64 l2[512]  __attribute__((aligned(4096)));

u64 *mmu_kernel_table(void) { return l1; }

void mmu_init(void) {
    /* L1[0] = 1GB Device */
    l1[0] = 0x00000000UL | PTE_BLOCK | PTE_ATTR(ATTR_DEVICE) | PTE_AF | PTE_UXN | PTE_PXN;
    /* L1[1] -> L2 of RAM (2MB per entry) */
    l1[1] = (u64)l2 | PTE_TABLE;
    u64 ram_blocks = RAM_SIZE >> 21;   // kernel (EL1) sees all RAM, including user portion
    for (u64 i = 0; i < ram_blocks; i++)
        l2[i] = (RAM_BASE + (i << 21)) | PTE_BLOCK | PTE_ATTR(ATTR_NORMAL) | PTE_SH_INNER | PTE_AF | PTE_UXN;

    u64 mair = (0x00UL << (8 * ATTR_DEVICE)) | (0xFFUL << (8 * ATTR_NORMAL));
    u64 mmfr0; __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(mmfr0));
    u64 tcr = 25UL                  // T0SZ: VA 39 bit
            | (1UL << 8)            // IRGN0 = WB WA
            | (1UL << 10)           // ORGN0 = WB WA
            | (3UL << 12)           // SH0 = inner shareable
            | (0UL << 14)           // TG0 = 4KB
            | (1UL << 23)           // EPD1: Disable TTBR1
            | ((mmfr0 & 7UL) << 32);// IPS = PARange

    __asm__ volatile(
        "msr mair_el1, %0\n msr tcr_el1, %1\n msr ttbr0_el1, %2\n"
        "dsb ish\n isb\n tlbi vmalle1\n dsb ish\n isb\n ic iallu\n dsb ish\n isb\n"
        "mrs x9, sctlr_el1\n"
        "orr x9, x9, #(1 << 0)\n"       // M: Enable MMU
        "orr x9, x9, #(1 << 2)\n"       // C: data cache
        "orr x9, x9, #(1 << 12)\n"      // I: instruction cache
        "msr sctlr_el1, x9\n isb\n"
        :: "r"(mair), "r"(tcr), "r"((u64)l1) : "x9", "memory");
}

void mmu_switch(u64 *table) {
    __asm__ volatile("msr ttbr0_el1, %0\n isb\n tlbi vmalle1\n dsb ish\n isb\n"
                    :: "r"((u64)table) : "memory");
}

/* Create new address space: same as kernel + user page table at VA 0x80000000 */
u64 *mmu_new_user_table(u64 user_phys) {
    u64 *t1 = pmm_alloc(1);
    u64 *t2 = pmm_alloc(1);
    if (!t1 || !t2) return 0;
    memcpy(t1, l1, sizeof(l1));
    t2[0] = user_phys | PTE_BLOCK | PTE_ATTR(ATTR_NORMAL) | PTE_SH_INNER | PTE_AF | PTE_AP_USER | PTE_PXN;
    t1[(USER_VBASE >> 30) & 511] = (u64)t2 | PTE_TABLE;
    return t1;
}
