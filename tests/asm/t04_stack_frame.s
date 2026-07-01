# t04_stack_frame.s - build a "yield frame" by hand and resume from it.
#
# Learning objective: one step past t03_ctx_save_restore. SSOS's ss_task_yield
# does not just save registers — it pushes a *frame* (return PC + SR + registers)
# and resumes by popping them back and jumping to the saved PC. This sample
# builds that exact frame manually, trashes everything, then unwinds it the way
# .resume_task does for a yielded task:
#
#   push order (top of stack last):  regs (movem.l)  SR  return-PC (pea)
#   pop  order:                      regs (movem.l)  SR  PC -> jmp (a0)
#
# If the frame is built or unwound in the wrong order, registers or the resume
# target come back wrong and we print FAIL.

.equ TTY, 0xff008000

.text
.global _start

_start:
    move.l  #0x4000, %sp

    | load distinct patterns so we can detect corruption
    move.l  #0x11111111, %d0
    move.l  #0x22222222, %d1
    move.l  #0x33333333, %d2
    move.l  #0x44444444, %d3
    move.l  #0x55555555, %d4
    move.l  #0x66666666, %d5
    move.l  #0x77777777, %d6
    move.l  #0x88888888, %d7
    move.l  #0x10001000, %a0
    move.l  #0x20002000, %a1
    move.l  #0x30003000, %a2
    move.l  #0x40004000, %a3
    move.l  #0x50005000, %a4
    move.l  #0x60006000, %a5

    | ---- build the yield frame (same shape as ss_task_yield) ----
    pea     .resume             | return PC sits at the bottom of the frame
    move.w  #0x2000, -(%sp)     | SR to restore (interrupts on)
    movem.l %d0-%d7/%a0-%a5, -(%sp)   | save caller regs (a6 is frame ptr; a7 is sp)

    | ---- trash everything ----
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

    | ---- unwind the frame (same shape as .resume_task for a yielded task) ----
    movem.l (%sp)+, %d0-%d7/%a0-%a5
    move.w  (%sp)+, %sr         | restore SR
    move.l  (%sp)+, %a6         | saved PC (we used a6 as scratch for the jump)
    jmp     (%a6)               | "return" to .resume

.resume:
    | ---- verify the patterns survived the round trip ----
    cmpl    #0x11111111, %d0
    bne     fail
    cmpl    #0x88888888, %d7
    bne     fail
    cmpl    #0x10001000, %a0
    bne     fail
    cmpl    #0x60006000, %a5
    bne     fail

    lea     ok_msg, %a0
    bsr     print_str
    bra     halt

fail:
    lea     fail_msg, %a0
    bsr     print_str

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

ok_msg:   .asciz "t04: OK\n"
fail_msg: .asciz "t04: FAIL\n"
