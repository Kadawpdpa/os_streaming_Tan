.macro ventry target
.balign 0x80
b \target
.endm

.macro save_regs
    sub   sp, sp, #256
    stp   x0,  x1,  [sp, #0]
    stp   x2,  x3,  [sp, #16]
    stp   x4,  x5,  [sp, #32]
    stp   x6,  x7,  [sp, #48]
    stp   x8,  x9,  [sp, #64]
    stp   x10, x11, [sp, #80]
    stp   x12, x13, [sp, #96]
    stp   x14, x15, [sp, #112]
    stp   x16, x17, [sp, #128]
    stp   x18, x19, [sp, #144]
    stp   x20, x21, [sp, #160]
    stp   x22, x23, [sp, #176]
    stp   x24, x25, [sp, #192]
    stp   x26, x27, [sp, #208]
    stp   x28, x29, [sp, #224]
    str   x30,      [sp, #240]
.endm

.macro restore_regs
    ldp   x0,  x1,  [sp, #0]
    ldp   x2,  x3,  [sp, #16]
    ldp   x4,  x5,  [sp, #32]
    ldp   x6,  x7,  [sp, #48]
    ldp   x8,  x9,  [sp, #64]
    ldp   x10, x11, [sp, #80]
    ldp   x12, x13, [sp, #96]
    ldp   x14, x15, [sp, #112]
    ldp   x16, x17, [sp, #128]
    ldp   x18, x19, [sp, #144]
    ldp   x20, x21, [sp, #160]
    ldp   x22, x23, [sp, #176]
    ldp   x24, x25, [sp, #192]
    ldp   x26, x27, [sp, #208]
    ldp   x28, x29, [sp, #224]
    ldr   x30,      [sp, #240]
    add   sp, sp, #256
.endm

.section ".text.vectors"
.balign 0x800
.global vectors
vectors:
    ventry bad_entry
    ventry bad_entry
    ventry bad_entry
    ventry bad_entry
    ventry bad_entry
    ventry irq_entry
    ventry bad_entry
    ventry bad_entry
    .rept 8
    ventry bad_entry
    .endr

bad_entry:
    bl    handle_unexpected
    b     .

irq_entry:
    save_regs
    mrs   x0, elr_el1          // Save the return address + CPU state of this thread
    mrs   x1, spsr_el1         // (other threads might overwrite these values while we are swapped out)
    stp   x0, x1, [sp, #-16]!
    bl    handle_irq
    ldp   x0, x1, [sp], #16
    msr   elr_el1, x0
    msr   spsr_el1, x1
    restore_regs
    eret
