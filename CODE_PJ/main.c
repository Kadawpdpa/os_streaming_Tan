#define UART0 ((volatile unsigned int *)0x09000000)  // PL011 ของเครื่อง virt

static void putc(char c) { *UART0 = c; }

static void puts(const char *s) {
    while (*s) {
        if (*s == '\n') putc('\r');
        putc(*s++);
    }
}

void kmain(void) {
    puts("Hello from my own kernel on AArch64!\n");
    for (;;) __asm__ volatile("wfe");
}
