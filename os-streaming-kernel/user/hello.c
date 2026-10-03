/* User mode program (EL0): Communicates with the kernel via system calls only */
static long sys(long n, long a, long b, long c) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2) : "memory");
    return x0;
}
static void puts(const char *s) { long n = 0; while (s[n]) n++; sys(1, 1, (long)s, n); }
static void putnum(unsigned long v) {
    char b[24]; int i = 23; b[i] = 0;
    if (!v) b[--i] = '0';
    while (v) { b[--i] = '0' + v % 10; v /= 10; }
    puts(&b[i]);
}
static void puthex(unsigned long v) {
    char b[20]; int i = 19; b[i] = 0;
    if (!v) b[--i] = '0';
    while (v) { b[--i] = "0123456789abcdef"[v & 15]; v >>= 4; }
    puts("0x"); puts(&b[i]);
}

static unsigned long counter;          /* Global variable: Same VA for all processes, but the value is process-specific */

int main(void) {
    long pid = sys(5, 0, 0, 0);
    for (int i = 1; i <= 5; i++) {
        counter += (unsigned long)pid * 100;
        puts("  [user pid "); putnum((unsigned long)pid);
        puts("] step ");      putnum((unsigned long)i);
        puts(", &counter=");  puthex((unsigned long)&counter);
        puts(" value=");      putnum(counter);
        puts("\n");
        sys(4, 200, 0, 0);             /* sleep 200 ms */
    }
    return 0;
}