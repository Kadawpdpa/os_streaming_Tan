#ifndef THREAD_H
#define THREAD_H
int  thread_create(void (*fn)(void));
void yield(void);
int  thread_self(void);
int  threads_running(void);

// Disable/Return state of IRQ: Use for the critical section
static inline unsigned long irq_save(void) {
    unsigned long f;
    __asm__ volatile("mrs %0, daif\n msr daifset, #2" : "=r"(f) :: "memory");
    return f;
}
static inline void irq_restore(unsigned long f) {
    __asm__ volatile("msr daif, %0" :: "r"(f) : "memory");
}
#endif
