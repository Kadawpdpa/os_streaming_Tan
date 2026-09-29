.section ".text.boot"
.global _start
_start:
    mrs   x0, mpidr_el1
    and   x0, x0, #0xff
    cbnz  x0, hang            // ปล่อยให้แค่ core 0 ทำงาน
    ldr   x0, =stack_top
    mov   sp, x0
    ldr   x0, =__bss_start    // เคลียร์ .bss
    ldr   x1, =__bss_end
1:  cmp   x0, x1
    b.hs  2f
    str   xzr, [x0], #8
    b     1b
2:  bl    kmain
hang:
    wfe
    b     hang

.section ".bss"
.balign 16
stack_bottom:
    .skip 16384
stack_top:
