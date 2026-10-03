/* kmalloc/kfree: first-fit on a 4MB heap with adjacent free block merging */
#include "kernel.h"
#include <string.h>

#define HEAP_PAGES 1024                  /* 4 MB */
struct blk { size_t size; int free; struct blk *next; };
#define HDR  ((sizeof(struct blk) + 15) & ~15UL)

static struct blk *head;
static size_t used_bytes;

void kheap_init(void) {
    head = pmm_alloc(HEAP_PAGES);
    if (!head) panic("kheap_init: out of memory");
    head->size = HEAP_PAGES * PAGE_SIZE - HDR;
    head->free = 1; head->next = 0;
}

void *kmalloc(size_t n) {
    n = (n + 15) & ~15UL;
    unsigned long f = irq_save();
    for (struct blk *b = head; b; b = b->next) {
        if (!b->free || b->size < n) continue;
        if (b->size >= n + HDR + 16) {                 /* Split the block */
            struct blk *r = (struct blk *)((char *)b + HDR + n);
            r->size = b->size - n - HDR; r->free = 1; r->next = b->next;
            b->size = n; b->next = r;
        }
        b->free = 0; used_bytes += b->size;
        irq_restore(f);
        return (char *)b + HDR;
    }
    irq_restore(f);
    return 0;
}

void kfree(void *p) {
    if (!p) return;
    unsigned long f = irq_save();
    struct blk *b = (struct blk *)((char *)p - HDR);
    b->free = 1; used_bytes -= b->size;
    for (struct blk *c = head; c; c = c->next)         /* Merge adjacent free blocks */
        while (c->free && c->next && c->next->free) { c->size += HDR + c->next->size; c->next = c->next->next; }
    irq_restore(f);
}
size_t kheap_used(void) { return used_bytes; }