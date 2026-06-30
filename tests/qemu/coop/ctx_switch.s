# ctx_switch.s - QEMU-port of the SSOS cooperative context switch.
#
# This is a direct port of ssos/os/kernel/cooperative/interrupts.s lines
# 17-23 (irq disable/enable) and 280-344 (ss_task_yield / .resume_task /
# .start_task), with every X68000-MFP / Human68K-TRAP dependency removed.
# What remains is pure 68000: stack switching via movem.l. It runs unchanged
# on qemu-system-m68k -M virt.
#
# SSTask layout (m68k ABI, no packing needed — matches scheduler.h):
#   +0   context     (saved SP)
#   +12  stack_base
#   +20  entry
#   +31  pad         (used as resume_type: 1 = yielded, 0 = interrupted)

        .section .text
        .align  2

        .globl  _start
        .globl  ss_disable_interrupts, ss_enable_interrupts
        .globl  ss_task_yield, ss_do_context_switch
        .globl  ss_curr_task, ss_scheduled_task

# ----------------------------------------------------------------------------
# _start: QEMU entry. Set up a master stack, call C main, then halt.
# ----------------------------------------------------------------------------
_start:
        move.l  #0x10000, %sp
        move.w  #0x2000, %sr        | interrupts on (harmless on QEMU virt)
        bsr     main
        | main returned — park forever
1:      bra     1b

# ----------------------------------------------------------------------------
# Interrupt enable/disable (real SR manipulation; QEMU handles SR correctly).
# ----------------------------------------------------------------------------
ss_disable_interrupts:
        move.w  #0x2700, %sr
        rts

ss_enable_interrupts:
        move.w  #0x2000, %sr
        rts

# ----------------------------------------------------------------------------
# ss_task_yield - voluntary context switch (callable from C).
# Build a manual resume frame (return PC + SR), save all registers, mark the
# current task as yielded, save its SP, let the C scheduler pick the next task,
# then resume whatever it chose.
# ----------------------------------------------------------------------------
        .globl  ss_task_yield
ss_task_yield:
        pea     .yield_resume        | return PC for when we come back
        move.w  #0x2000, -(sp)       | SR to restore (interrupts on)
        movem.l d0-d7/a0-a6, -(sp)   | save ALL regs (resume in another context)

        move.l  ss_curr_task, a1
        move.b  #1, 31(a1)           | resume_type = 1 (yielded)
        move.l  sp, (a1)             | curr_task->context = SP

        bsr     ss_do_context_switch | C scheduler picks ss_scheduled_task
        bra     .resume_task

.yield_resume:
        rts

# ----------------------------------------------------------------------------
# .resume_task - switch to ss_scheduled_task's saved stack and resume it.
# Three cases:
#   * new task      (context == stack_base) -> jump to entry
#   * yielded       (resume_type == 1)      -> pop regs/SR/PC, jmp (no rte)
#   * interrupted   (resume_type == 0)      -> pop regs, rte  [unused here]
# ----------------------------------------------------------------------------
.resume_task:
        move.l  ss_scheduled_task, a1
        move.l  (a1), sp             | adopt the target task's stack

        | new-task test: context == stack_base?
        move.l  (a1), a0
        move.l  12(a1), d0
        cmp.l   a0, d0
        beq.w   .start_task

        | resume_type at offset 31
        cmpi.b  #0, 31(a1)
        beq.s   .resume_interrupted

        | yielded: manual SR/PC restore
        movem.l (sp)+, d0-d7/a0-a6
        move.w  (sp)+, %sr
        move.l  (sp)+, %a0
        jmp     (%a0)

.resume_interrupted:
        | interrupted (CPU frame intact) — unused in cooperative-only test
        movem.l (sp)+, d0-d7/a0-a6
        rte

.start_task:
        | first run of a task: set SR and jump straight to entry
        move.l  20(a1), a0           | a0 = task entry function
        move.w  #0x2000, %sr
        jmp     (%a0)
