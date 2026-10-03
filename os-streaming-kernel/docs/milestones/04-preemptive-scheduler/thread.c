#include "thread.h"

#define MAX_THREADS 8
#define STACK_SIZE  4096

struct context {                 // The order must match switch.S
    unsigned long x19, x20, x21, x22, x23, x24, x25, x26, x27, x28;
    unsigned long fp, lr, sp;
};

struct thread {
    struct context ctx;
    int alive;
};

extern void cpu_switch(struct context *prev, struct context *next);
extern void thread_trampoline(void);

static struct thread threads[MAX_THREADS] = { [0] = { .alive = 1 } };  // thread 0 = kmain
static unsigned char stacks[MAX_THREADS][STACK_SIZE] __attribute__((aligned(16)));
static int nthreads = 1;
static int current = 0;

int thread_create(void (*fn)(void)) {
    unsigned long f = irq_save();
    if (nthreads >= MAX_THREADS) { irq_restore(f); return -1; }
    int id = nthreads++;
    threads[id].ctx.x19 = (unsigned long)fn;
    threads[id].ctx.lr  = (unsigned long)thread_trampoline;
    threads[id].ctx.sp  = (unsigned long)&stacks[id][STACK_SIZE];
    threads[id].alive   = 1;
    irq_restore(f);
    return id;
}

int thread_self(void) { return current; }

int threads_running(void) {
    int n = 0;
    for (int i = 0; i < nthreads; i++) n += threads[i].alive;
    return n;
}

// Round-robin scheduler: can be called both from a thread (voluntarily yielding CPU) and from a timer IRQ (preemption)
void yield(void) {
    unsigned long f = irq_save();          // Prevent nested IRQs during the context switch
    int prev = current, next = current;
    do {
        next = (next + 1) % nthreads;
    } while (!threads[next].alive && next != prev);
    if (next != prev) {
        current = next;
        cpu_switch(&threads[prev].ctx, &threads[next].ctx);
    }
    irq_restore(f);                        // Execution resumes here when this thread regains the CPU
}

void thread_exit(void) {
    unsigned long f = irq_save();
    threads[current].alive = 0;
    irq_restore(f);
    yield();
    for (;;) __asm__ volatile("wfe");
}