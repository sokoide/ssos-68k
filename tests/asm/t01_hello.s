# t01_hello.s - smallest possible m68k program for QEMU virt.
#
# Learning objective: the bare minimum. Set a stack, write each byte of a
# string to the Goldfish TTY (0xff008000), halt. No subroutines, no macros —
# just a loop and a memory-mapped register. Confirms the QEMU toolchain works.
#
# Compare with t02_subroutines.s (adds bsr/rts) and t03_ctx_save_restore.s
# (adds movem.l register save/restore).

.equ TTY, 0xff008000

.text
.global _start

_start:
    move.l  #0x4000, %sp       | set up a stack (QEMU virt RAM at 0x0)
    lea     msg, %a0            | a0 -> string

loop:
    move.b  (%a0)+, %d0         | load next byte, advance pointer
    beq     done                | NUL -> end of string
    move.l  %d0, TTY            | write byte to Goldfish TTY
    bra     loop

done:
    bra     done                | park forever (QEMU timeout reaps us)

msg:
    .asciz "Hello, m68k!\n"
