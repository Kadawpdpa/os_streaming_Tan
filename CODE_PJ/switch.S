// void cpu_switch(struct context *prev, struct context *next)
.text
.global cpu_switch
cpu_switch:
    mov   x9, sp
    stp   x19, x20, [x0, #0]
    stp   x21, x22, [x0, #16]
    stp   x23, x24, [x0, #32]
    stp   x25, x26, [x0, #48]
    stp   x27, x28, [x0, #64]
    stp   x29, x30, [x0, #80]
    str   x9,       [x0, #96]
    ldp   x19, x20, [x1, #0]
    ldp   x21, x22, [x1, #16]
    ldp   x23, x24, [x1, #32]
    ldp   x25, x26, [x1, #48]
    ldp   x27, x28, [x1, #64]
    ldp   x29, x30, [x1, #80]
    ldr   x9,       [x1, #96]
    mov   sp, x9
    ret

// thread ใหม่จะเริ่มที่นี่ (x19 = ฟังก์ชันที่จะรัน)
.global thread_trampoline
thread_trampoline:
    msr   daifclr, #2          // เปิดรับ IRQ (thread ใหม่เริ่มมาพร้อม IRQ ที่ถูกปิด)
    blr   x19
    bl    thread_exit
    b     .
