/* Kernel threads + round-robin scheduler (preemptive via timer interrupt) */
#include "kernel.h"
#include <string.h>

#define MAX_THREADS  16
#define KSTACK_PAGES 4                     /* 16 KB per thread */

enum { T_FREE, T_READY, T_SLEEP, T_DEAD };

struct context {                           /* The order must match arch/switch.S */
    u64 x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    u64 fp, lr, sp;
};

struct thread {
    struct context ctx;
    int   state;
    u64   wake_ms;
    u64  *ttbr0;                           /* 0 = use kernel page table */
    void *kstack;
    int   user_slot;                       /* -1 = not a user process */
    u64   run_ticks;
    char  name[16];
};

extern void cpu_switch(struct context *prev, struct context *next);
extern void thread_trampoline(void);

static struct thread threads[MAX_THREADS];
static int current;
static u64 *cur_table;
static u8 slot_used[USER_SLOTS];

void thread_init(void) {
    memset(threads, 0, sizeof(threads));
    threads[0].state = T_READY;            /* thread 0 = kmain, acts as idle */
    threads[0].user_slot = -1;
    strncpy(threads[0].name, "idle", 15);
    cur_table = mmu_kernel_table();
}

static void reap(struct thread *t) {       /* Reclaim resources of an exited thread */
    if (t->ttbr0) {
        u64 l2 = t->ttbr0[(USER_VBASE >> 30) & 511] & ~0xfffUL;
        pmm_free((void *)l2, 1);
        pmm_free(t->ttbr0, 1);
        t->ttbr0 = 0;
    }
    if (t->user_slot >= 0) { slot_used[t->user_slot] = 0; t->user_slot = -1; }
}

static int create(void (*fn)(void *), void *arg, const char *name, u64 *table, int slot) {
    unsigned long f = irq_save();
    int id = -1;
    for (int i = 1; i < MAX_THREADS; i++)
        if (threads[i].state == T_FREE || threads[i].state == T_DEAD) { id = i; break; }
    if (id < 0) { irq_restore(f); return -1; }
    struct thread *t = &threads[id];
    if (t->state == T_DEAD) reap(t);
    if (!t->kstack) t->kstack = pmm_alloc(KSTACK_PAGES);
    if (!t->kstack) { irq_restore(f); return -1; }
    memset(&t->ctx, 0, sizeof(t->ctx));
    t->ctx.x19 = (u64)fn;
    t->ctx.x20 = (u64)arg;
    t->ctx.lr  = (u64)thread_trampoline;
    t->ctx.sp  = (u64)t->kstack + KSTACK_PAGES * PAGE_SIZE;
    t->ttbr0 = table; t->user_slot = slot; t->run_ticks = 0; t->wake_ms = 0;
    strncpy(t->name, name, 15); t->name[15] = 0;
    t->state = T_READY;
    irq_restore(f);
    return id;
}

int thread_create(void (*fn)(void *), void *arg, const char *name) {
    return create(fn, arg, name, 0, -1);
}

int thread_self(void) { return current; }

/* Can be called both from a thread (voluntarily yielding CPU) and from a timer IRQ (preemption) */
void yield(void) {
    unsigned long f = irq_save();
    u64 now = uptime_ms();
    int prev = current, next = 0;
    for (int k = 1; k <= MAX_THREADS; k++) {
        int i = (prev + k) % MAX_THREADS;
        if (i == 0) continue;
        struct thread *t = &threads[i];
        if (t->state == T_SLEEP && now >= t->wake_ms) t->state = T_READY;
        if (t->state == T_READY) { next = i; break; }
    }
    if (next == 0 && prev != 0 && threads[prev].state == T_READY) next = prev;
    if (next != prev) {
        struct thread *n = &threads[next];
        u64 *tbl = n->ttbr0 ? n->ttbr0 : mmu_kernel_table();
        if (tbl != cur_table) { mmu_switch(tbl); cur_table = tbl; }
        current = next;
        cpu_switch(&threads[prev].ctx, &n->ctx);
    }
    irq_restore(f);
}

void thread_sleep_ms(unsigned ms) {
    unsigned long f = irq_save();
    threads[current].wake_ms = uptime_ms() + ms;
    threads[current].state = T_SLEEP;
    irq_restore(f);
    yield();
}

void thread_exit(void) {
    unsigned long f = irq_save();
    threads[current].state = T_DEAD;
    irq_restore(f);
    yield();
    for (;;) __asm__ volatile("wfe");
}

void thread_tick(void) { threads[current].run_ticks++; }

int threads_running(void) {
    int n = 0;
    for (int i = 0; i < MAX_THREADS; i++) n += (threads[i].state == T_READY || threads[i].state == T_SLEEP);
    return n;
}

void thread_dump(void) {
    static const char *st[] = { "free", "ready", "sleep", "dead" };
    kprintf("  ID  NAME             STATE  CPU(ticks)  KIND\n");
    for (int i = 0; i < MAX_THREADS; i++) {
        struct thread *t = &threads[i];
        if (t->state == T_FREE) continue;
        kprintf("  %2d  %-16s %-6s %10lu  %s%s\n", i, t->name, st[t->state], (unsigned long)t->run_ticks,
                t->ttbr0 ? "user process" : "kernel thread", i == current ? "  <- running" : "");
    }
}

/* ---------- user process ---------- */
extern void enter_user(u64 entry, u64 sp);
static void user_entry(void *arg) {
    (void)arg;
    enter_user(USER_VBASE, USER_STACK_TOP);
}

int proc_exec(const void *img, size_t len, const char *name) {
    if (len > USER_SLOT_SIZE - 0x10000) return -1;
    unsigned long f = irq_save();
    int slot = -1;
    for (int i = 0; i < USER_SLOTS; i++) if (!slot_used[i]) { slot = i; break; }
    if (slot < 0) { irq_restore(f); return -2; }
    slot_used[slot] = 1;
    irq_restore(f);

    u64 phys = USER_PHYS_BASE + (u64)slot * USER_SLOT_SIZE;
    memset((void *)phys, 0, USER_SLOT_SIZE);
    memcpy((void *)phys, img, len);
    __asm__ volatile("dsb ish\n ic iallu\n dsb ish\n isb" ::: "memory");   /* Make the instruction cache see the new code */
    u64 *table = mmu_new_user_table(phys);
    if (!table) { slot_used[slot] = 0; return -3; }
    int id = create(user_entry, 0, name, table, slot);
    if (id < 0) { slot_used[slot] = 0; return -4; }
    return id;
}