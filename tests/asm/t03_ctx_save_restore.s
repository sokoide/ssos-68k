# t03_ctx_save_restore.s - movem.l register save/restore
#
# Learning objective: confirm that `movem.l %d0-%d7/%a0-%a6, -(%sp)` saves all
# 15 caller registers and `movem.l (%sp)+, %d0-%d7/%a0-%a6` restores them
# exactly. This pair is the core primitive the SSOS context switch (and any
# m68k preemptive kernel) uses to multiplex tasks onto one CPU.
#
# Method: load a distinct pattern into every register, push them all, trash
# every register to zero, pull them all back, then spot-check a few. Prints
# "OK\n" on success or "FAIL\n" if any check mismatches. Output goes to the
# QEMU virt Goldfish TTY at 0xff008000.

.equ TTY, 0xff008000

.text
.global _start

_start:
    move.l  #0x4000, %sp

    /* ---- load distinct patterns into d0-d7 ---- */
    move.l  #0x11111111, %d0
    move.l  #0x22222222, %d1
    move.l  #0x33333333, %d2
    move.l  #0x44444444, %d3
    move.l  #0x55555555, %d4
    move.l  #0x66666666, %d5
    move.l  #0x77777777, %d6
    move.l  #0x88888888, %d7

    /* ---- and a0-a6 (address regs; immediate-to-An is allowed) ---- */
    move.l  #0x10001000, %a0
    move.l  #0x20002000, %a1
    move.l  #0x30003000, %a2
    move.l  #0x40004000, %a3
    move.l  #0x50005000, %a4
    move.l  #0x60006000, %a5
    move.l  #0x70007000, %a6

    /* ---- save all 15 registers onto the stack ---- */
    movem.l %d0-%d7/%a0-%a6, -(%sp)

    /* ---- trash every register to zero ---- */
    move.l  #0, %d0
    move.l  #0, %d1
    move.l  #0, %d2
    move.l  #0, %d3
    move.l  #0, %d4
    move.l  #0, %d5
    move.l  #0, %d6
    move.l  #0, %d7
    move.l  #0, %a0
    move.l  #0, %a1
    move.l  #0, %a2
    move.l  #0, %a3
    move.l  #0, %a4
    move.l  #0, %a5
    move.l  #0, %a6

    /* ---- restore all 15 registers ---- */
    movem.l (%sp)+, %d0-%d7/%a0-%a6

    /* ---- spot-check representatives from each group ---- */
    cmpl    #0x11111111, %d0
    bne     fail
    cmpl    #0x88888888, %d7
    bne     fail
    cmpl    #0x10001000, %a0
    bne     fail
    cmpl    #0x70007000, %a6
    bne     fail

    lea     ok_msg, %a0
    bsr     print_str
    bra     halt

fail:
    lea     fail_msg, %a0
    bsr     print_str

halt:
    bra     halt

# ---- Subroutines ----

# print_str: print null-terminated string at a0 to the Goldfish TTY.
print_str:
    movem.l %d0/%a0, -(%sp)
.Lps_loop:
    move.b  (%a0)+, %d0
    tst.b   %d0
    beq     .Lps_done
    bsr     putchar
    bra     .Lps_loop
.Lps_done:
    movem.l (%sp)+, %d0/%a0
    rts

# putchar: write the low byte in d0 to the Goldfish TTY (leaf).
putchar:
    move.l  %d0, TTY
    rts

# ---- Data ----
ok_msg:
    .asciz "t03: OK\n"
fail_msg:
    .asciz "t03: FAIL\n"
