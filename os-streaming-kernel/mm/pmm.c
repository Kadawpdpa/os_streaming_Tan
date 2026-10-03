/* Physical memory manager: 4KB page bitmap, contiguous page allocation via first-fit */
#include "kernel.h"
#include <string.h>

extern char __kernel_end[];
#define PMM_END     USER_PHYS_BASE
#define MAX_PAGES   ((PMM_END - RAM_BASE) / PAGE_SIZE)

static u8 bitmap[MAX_PAGES / 8 + 1];     /* 1 = used */
static u64 first_page, npages, nfree;

static inline int  used(u64 i)  { return bitmap[i >> 3] & (1 << (i & 7)); }
static inline void mark(u64 i, int v) { if (v) bitmap[i >> 3] |= 1 << (i & 7); else bitmap[i >> 3] &= ~(1 << (i & 7)); }

void pmm_init(void) {
    u64 start = ((u64)__kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    first_page = start;
    npages = (PMM_END - start) / PAGE_SIZE;
    nfree = npages;
    memset(bitmap, 0, sizeof(bitmap));
}

void *pmm_alloc(size_t n) {
    unsigned long f = irq_save();
    u64 run = 0;
    for (u64 i = 0; i < npages; i++) {
        run = used(i) ? 0 : run + 1;
        if (run == n) {
            u64 s = i + 1 - n;
            for (u64 k = s; k <= i; k++) mark(k, 1);
            nfree -= n;
            irq_restore(f);
            void *p = (void *)(first_page + s * PAGE_SIZE);
            memset(p, 0, n * PAGE_SIZE);
            return p;
        }
    }
    irq_restore(f);
    return 0;
}

void pmm_free(void *p, size_t n) {
    unsigned long f = irq_save();
    u64 s = ((u64)p - first_page) / PAGE_SIZE;
    for (u64 k = s; k < s + n; k++) if (used(k)) { mark(k, 0); nfree++; }
    irq_restore(f);
}
size_t pmm_total_pages(void) { return npages; }
size_t pmm_free_pages(void)  { return nfree; }