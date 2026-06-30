# preempt_ctx_switch.s - QEMU-port of the SSOS preemptive Timer D path.
#
# Port of ssos/os/kernel/preemptive/interrupts.s:
#   - ss_timerd_handler (255-308)
#   - ss_context_switch (315-323)
#   - .resume_task / .resume_interrupted / .start_task (329-359)
#
# Timer D hardware does not exist on QEMU virt, so we drive the SAME handler
# via `trap #0`. A trap exception frame (SR 2B + PC 4B) is byte-identical to a
# Timer D interrupt frame, so the `.resume_interrupted` -> `rte` path runs
# exactly as on real hardware. This is the path Native tests and Phase B
# (cooperative) cannot reach: rte-based preemption.
#
# Differences from the real handler:
#   * entered via trap #0 (vector 0x80), not MFP Timer D (vector 0x110)
#   * MFP EOI writes (0xe88011 bit4 clear) removed - no MFP on QEMU virt
#   * switch threshold is 1 (switch every tick) for a clear 1212 pattern;
#     the real kernel uses 10 (see interrupts.s:264)
#
# SSTask layout: context=0, stack_base=12, entry=20, pad(resume_type)=31.

        .section .text
        .align  2

        .globl  _start
        .globl  ss_timerd_handler, ss_context_switch
        .globl  ss_curr_task, ss_scheduled_task
        .globl  ss_tick_counter, ss_timerd_fire_count
        .globl  ss_switch_tick, ss_context_switch_count
        .extern ss_do_context_switch
        .extern ss_do_wakeups
        .extern main

# ----------------------------------------------------------------------------
# _start: set up stack, install the trap #0 handler (vector at 0x80), run main.
# ----------------------------------------------------------------------------
_start:
        move.l  #0x10000, sp
        lea     ss_timerd_handler, a0
        move.l  a0, 0x80               | vector 32 (trap #0) -> our ISR
        move.w  #0x2000, %sr            | interrupts on
        bsr     main
1:      bra     1b

# ----------------------------------------------------------------------------
# Interrupt enable/disable (scheduler.c guards queue sections with these).
# ----------------------------------------------------------------------------
        .globl  ss_disable_interrupts, ss_enable_interrupts
ss_disable_interrupts:
        move.w  #0x2700, %sr
        rts

ss_enable_interrupts:
        move.w  #0x2000, %sr
        rts

# ----------------------------------------------------------------------------
# ss_task_yield - voluntary context switch (callable from C, e.g. ss_task_sleep).
# Builds a manual resume frame (PC + SR), saves all regs, marks resume_type=1,
# and resumes whatever the scheduler picks next.
# ----------------------------------------------------------------------------
        .globl  ss_task_yield
ss_task_yield:
        pea     .yield_resume
        move.w  #0x2000, -(sp)
        movem.l d0-d7/a0-a6, -(sp)
        move.l  ss_curr_task, a1
        move.b  #1, 31(a1)              | resume_type = 1 (yielded)
        move.l  sp, (a1)                | curr_task->context = SP
        bsr     ss_do_context_switch
        bra.w   .resume_task
.yield_resume:
        rts

# ----------------------------------------------------------------------------
# ss_timerd_handler - the Timer D ISR, driven here by trap #0.
# Saves regs, bumps counters, and on the switch tick hands off to the
# scheduler via ss_context_switch. Non-switch ticks just rte back.
# ----------------------------------------------------------------------------
ss_timerd_handler:
        move.w  #0x2700, %sr            | mask interrupts (no nesting)
        movem.l d0/a0, -(sp)            | minimal save for the non-switch path
        addq.l  #1, ss_tick_counter
        addq.l  #1, ss_timerd_fire_count

        lea     ss_switch_tick, a0
        addq.b  #1, (a0)
        cmpi.b  #1, (a0)                | test: switch every tick (real kernel: 10)
        bne.s   .no_switch

        | switch tick: drop minimal save, do a full save
        movem.l (sp)+, d0/a0
        movem.l d0-d7/a0-a6, -(sp)
        lea     ss_switch_tick, a0
        move.b  #0, (a0)
        addq.l  #1, ss_context_switch_count

        bsr     ss_do_wakeups           | reap any timed-wait tasks

        move.l  ss_curr_task, d0
        beq.s   .no_switch_full         | no current task yet -> just return

        | (real kernel clears MFP ISRB bit4 here; no MFP on QEMU)
        move.l  ss_curr_task, a1
        move.b  #0, 31(a1)              | resume_type = 0 (interrupted)

        bra.w   ss_context_switch

.no_switch:
        movem.l (sp)+, d0/a0
        move.w  #0x2000, %sr
        rte

.no_switch_full:
        movem.l (sp)+, d0-d7/a0-a6
        move.w  #0x2000, %sr
        rte

# ----------------------------------------------------------------------------
# ss_context_switch - save current SP, let C pick the next task, resume it.
# ----------------------------------------------------------------------------
ss_context_switch:
        move.l  ss_curr_task, a1
        move.l  sp, (a1)                | curr_task->context = SP
        bsr     ss_do_context_switch    | C scheduler sets ss_scheduled_task
        | fall through to .resume_task

# ----------------------------------------------------------------------------
# .resume_task - adopt the scheduled task's stack and resume.
#   new task     (context == stack_base) -> jmp entry
#   resume_type 1 (yielded)             -> pop regs/SR/PC, jmp
#   resume_type 0 (interrupted)         -> pop regs, rte   <-- this test's target
# ----------------------------------------------------------------------------
.resume_task:
        move.l  ss_scheduled_task, a1
        move.l  (a1), sp

        move.l  (a1), a0
        move.l  12(a1), d0
        cmp.l   a0, d0
        beq.w   .start_task

        cmpi.b  #0, 31(a1)
        beq.s   .resume_interrupted

        movem.l (sp)+, d0-d7/a0-a6
        move.w  (sp)+, %sr
        move.l  (sp)+, %a0
        jmp     (%a0)

.resume_interrupted:
        movem.l (sp)+, d0-d7/a0-a6
        rte                             | CPU pops the SR+PC exception frame

.start_task:
        move.l  20(a1), a0              | a0 = entry
        move.w  #0x2000, %sr
        jmp     (%a0)

# ----------------------------------------------------------------------------
        .section .bss
        .align  2
ss_switch_tick:
        .skip   1
        .align  2
ss_context_switch_count:
        .skip   4
