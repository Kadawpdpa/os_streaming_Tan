/* PL011 UART (QEMU virt: 0x09000000) */
#include "kernel.h"
#define UART_BASE 0x09000000UL
#define UART_DR   (UART_BASE + 0x00)
#define UART_FR   (UART_BASE + 0x18)
#define FR_RXFE   (1u << 4)
#define FR_TXFF   (1u << 5)

void uart_putc(char c) {
    while (rd32(UART_FR) & FR_TXFF) { }
    wr32(UART_DR, (u32)c);
}
int uart_getc(void) {
    if (rd32(UART_FR) & FR_RXFE) return -1;
    return (int)(rd32(UART_DR) & 0xff);
}
