# t02_subroutines.s - subroutines (bsr/rts) and a counting loop.
#
# Learning objective: one step up from t01_hello. Factor putchar and print_str
# into subroutines called via bsr/rts, then print a numbered list (1..3) using a
# small decimal-printing subroutine. This is the m68k calling-convention and
# stack primer that t03_ctx_save_restore.s builds on.
#
# Reuses the Goldfish TTY at 0xff008000.

.equ TTY, 0xff008000

.text
.global _start

_start:
    move.l  #0x4000, %sp

    lea     msg1, %a0
    bsr     print_str           | "subroutines:"

    | print the lines "1)", "2)", "3)"
    move.l  #1, %d1             | d1 = counter
count_loop:
    bsr     print_dec           | print d1 as a decimal digit
    move.b  #')', %d0
    bsr     putchar
    move.b  #' ', %d0
    bsr     putchar
    lea     msg_line, %a0
    bsr     print_str

    addq.l  #1, %d1
    cmp.l   #4, %d1
    blt     count_loop

    lea     msg2, %a0
    bsr     print_str           | "done"

halt:
    bra     halt

# ---- subroutines ----

# putchar: write low byte of d0 to the Goldfish TTY. Leaf (no calls).
putchar:
    move.l  %d0, TTY
    rts

# print_str: print NUL-terminated string at a0.
print_str:
    movem.l %d0/%a0, -(%sp)     | save clobbered regs
.ps_loop:
    move.b  (%a0)+, %d0
    beq     .ps_done
    bsr     putchar
    bra     .ps_loop
.ps_done:
    movem.l (%sp)+, %d0/%a0
    rts

# print_dec: print d1 (1..9) as one decimal digit.
print_dec:
    move.b  #'0', %d0
    add.b   %d1, %d0            | '0' + d1
    bra     putchar             | tail-call (putchar returns to our caller)

# ---- data ----
msg1:    .asciz "subroutines:\n"
msg_line: .asciz "line\n"
msg2:    .asciz "done\n"
