# t05_trap_exception.s - trap #0 -> exception handler -> rte.
#
# Learning objective: the primitive the preemptive context switch is built on.
# A hardware interrupt (or trap) makes the CPU push an exception frame
# (SR 2B + PC 4B) and jump through a vector. `rte` pops that frame and resumes.
# This is exactly what SSOS's preemptive Timer D ISR uses; here it is in
# isolation, with trap #0 standing in for the interrupt.
#
# Flow:
#   _start: install handler at vector 0x80 (trap #0), print "before", trap #0,
#           print "after", print "OK".
#   handler: print "ISR", then `rte` returns to the instruction after `trap`.
# Seeing "before ISR after OK" proves the CPU pushed/popped the exception frame
# and came back to the right place.

.equ TTY, 0xff008000

.text
.global _start

_start:
    move.l  #0x4000, %sp

    | install our handler at trap #0 vector (vector 32 -> address 0x80)
    lea     trap_handler, %a0
    move.l  %a0, 0x80

    lea     msg_before, %a0
    bsr     print_str

    trap    #0                       | -> CPU pushes SR+PC, jumps via 0x80

    lea     msg_after, %a0
    bsr     print_str
    lea     msg_ok, %a0
    bsr     print_str
    bra     halt

# trap #0 handler. The exception frame (SR+PC) is already on the stack.
trap_handler:
    movem.l %d0/%a0, -(%sp)          | save what we clobber
    lea     msg_isr, %a0
    bsr     print_str_in_handler     | nested-safe printer (uses only d0/a0)
    movem.l (%sp)+, %d0/%a0
    rte                              | pop SR+PC, resume after `trap`

halt:
    bra     halt

# ---- helpers ----
print_str:
    movem.l %d0/%a0, -(%sp)
.ps_loop:
    move.b  (%a0)+, %d0
    tst.b   %d0
    beq     .ps_done
    move.l  %d0, TTY
    bra     .ps_loop
.ps_done:
    movem.l (%sp)+, %d0/%a0
    rts

# Same as print_str but doesn't touch d0/a0 (the caller already saved them).
print_str_in_handler:
.ps2_loop:
    move.b  (%a0)+, %d0
    tst.b   %d0
    beq     .ps2_done
    move.l  %d0, TTY
    bra     .ps2_loop
.ps2_done:
    rts

msg_before: .asciz "before "
msg_isr:    .asciz "ISR "
msg_after:  .asciz "after "
msg_ok:     .asciz "OK\n"
