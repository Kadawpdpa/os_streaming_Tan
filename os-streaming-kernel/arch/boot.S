.section ".text.boot"
.global _start
_start:
    mrs   x0, mpidr_el1
    and   x0, x0, #0xff
    cbnz  x0, park

    mrs   x0, CurrentEL
    lsr   x0, x0, #2
    cmp   x0, #2
    b.ne  el1_entry

    //EL2 -> EL1
    mrs   x0, cnthctl_el2
    orr   x0, x0, #3 //EL1 can use timer
    msr   cnthctl_el2, x0
    msr   cntvoff_el2, xzr
    mov   x0, #(1 << 31) //HCR_EL2.RW = EL1 on AArch64
    msr   hcr_el2, x0
    ldr   x0, =0x30d00800 //SCTLR_EL1 Default value (Disable MMU)
    msr   sctlr_el1, x0
    mov   x0, #0x3c5 //SPSR: EL1h, Disable DAIF
    msr   spsr_el2, x0
    adr   x0, el1_entry
    msr   elr_el2, x0
    eret

el1_entry:
    ldr   x0, =boot_stack_top
    mov   sp, x0
    ldr   x0, =__bss_start
    ldr   x1, =__bss_end
1:  cmp   x0, x1
    b.hs  2f
    str   xzr, [x0], #8
    b     1b
2:  bl    kmain
park:
    wfe
    b     park

.section ".bss"
.balign 16
boot_stack_bottom:
    .skip 16384
.global boot_stack_top
boot_stack_top:
