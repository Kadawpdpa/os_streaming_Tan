// Exception vector table + trap frame (struct trapframe in kernel.h = 288 bytes)
#define TF_SIZE 288

.macro ventry target
.balign 0x80
b \target
.endm

.macro kernel_entry
    sub   sp, sp, #TF_SIZE
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
    mrs   x0, elr_el1
    str   x0, [sp, #248]
    mrs   x0, spsr_el1
    str   x0, [sp, #256]
    mrs   x0, sp_el0
    str   x0, [sp, #264]
.endm

.macro kernel_exit
    ldr   x0, [sp, #248]
    msr   elr_el1, x0
    ldr   x0, [sp, #256]
    msr   spsr_el1, x0
    ldr   x0, [sp, #264]
    msr   sp_el0, x0
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
    add   sp, sp, #TF_SIZE
    eret
.endm

.section ".text.vectors"
.balign 0x800
.global vectors
vectors:
    // Current EL with SP0
    ventry bad_entry
    ventry bad_entry
    ventry bad_entry
    ventry bad_entry
    // Current EL with SPx (kernel)
    ventry sync_kernel_entry
    ventry irq_entry
    ventry bad_entry
    ventry bad_entry
    // Lower EL, AArch64 (user mode)
    ventry sync_user_entry
    ventry irq_entry
    ventry bad_entry
    ventry bad_entry
    // Lower EL, AArch32 (Does not support)
    ventry bad_entry
    ventry bad_entry
    ventry bad_entry
    ventry bad_entry

bad_entry:
    kernel_entry
    mov   x0, sp
    mov   x1, #0
    bl    handle_fatal
    b     .

sync_kernel_entry:
    kernel_entry
    mov   x0, sp
    mov   x1, #1
    bl    handle_fatal
    b     .

sync_user_entry:
    kernel_entry
    mov   x0, sp
    bl    handle_user_sync
    kernel_exit

irq_entry:
    kernel_entry
    bl    handle_irq
    kernel_exit
