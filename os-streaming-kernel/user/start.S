.section ".text.start"
.global _start
_start:
    bl    main
    mov   x8, #2            // SYS_EXIT (x0 = return value from main)
    svc   #0
1:  b     1b