/* system call from user mode (svc #0): syscall number in x8, arguments in x0-x2, result in x0 */
#include "kernel.h"

enum { SYS_WRITE = 1, SYS_EXIT = 2, SYS_YIELD = 3, SYS_SLEEP = 4, SYS_GETPID = 5, SYS_UPTIME = 6 };

static int user_range_ok(u64 p, u64 len) {
    return p >= USER_VBASE && len <= USER_SLOT_SIZE && p + len <= USER_STACK_TOP;
}

void user_syscall(struct trapframe *tf) {
    u64 n = tf->x[8], a0 = tf->x[0], a1 = tf->x[1], a2 = tf->x[2];
    long ret = -1;
    switch (n) {
    case SYS_WRITE:
        if (a0 == 1 && user_range_ok(a1, a2)) {
            const char *s = (const char *)a1;
            for (u64 i = 0; i < a2; i++) { if (s[i] == '\n') uart_putc('\r'); uart_putc(s[i]); }
            ret = (long)a2;
        }
        break;
    case SYS_EXIT:
        kprintf("[kernel] process %d exited with code %ld\n", thread_self(), (long)a0);
        thread_exit();
    case SYS_YIELD:  yield(); ret = 0; break;
    case SYS_SLEEP:  thread_sleep_ms((unsigned)a0); ret = 0; break;
    case SYS_GETPID: ret = thread_self(); break;
    case SYS_UPTIME: ret = (long)uptime_ms(); break;
    }
    tf->x[0] = (u64)ret;
}

void user_fault(struct trapframe *tf, u64 esr) {
    u64 far; __asm__ volatile("mrs %0, far_el1" : "=r"(far));
    kprintf("[kernel] process %d crashed: ESR=0x%lx (EC=0x%lx) ELR=0x%lx FAR=0x%lx -> killed\n",
            thread_self(), (unsigned long)esr, (unsigned long)(esr >> 26), (unsigned long)tf->elr, (unsigned long)far);
    thread_exit();
}