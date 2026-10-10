	| ctx_switch.s - QEMU-port of the SSOS cooperative context switch.
	|
	| This is a direct port of ssos/os/kernel/cooperative/interrupts.s lines
	| 17-23 (irq disable/enable) and 280-344 (ss_task_yield / .resume_task /
	| .start_task), with every X68000-MFP / Human68K-TRAP dependency removed.
	| What remains is pure 68000: stack switching via movem.l. It runs unchanged
	| on qemu-system-m68k -M virt.

#include "scheduler_abi.h"
	|
	| SSTask offsets used here come from scheduler_abi.h and are asserted against
	| the C layout by scheduler.h.

	.section .text
	.align   2

	.globl _start
	.globl ss_irq_save, ss_irq_restore
	.globl ss_task_yield, ss_do_context_switch
	.globl ss_curr_task, ss_scheduled_task

	| ----------------------------------------------------------------------------
	| _start: QEMU entry. Set up a master stack, call C main, then halt.
	| ----------------------------------------------------------------------------

_start:
	move.l #0x10000, %sp
	move.w #0x2000, %sr        | interrupts on (harmless on QEMU virt)
	bsr    main
	|      main returned — park forever

1:
	bra 1b

	| ----------------------------------------------------------------------------
	| Interrupt enable/disable (real SR manipulation; QEMU handles SR correctly).
	| ----------------------------------------------------------------------------

ss_irq_save:
	move.w %sr, %d0
	ori.w  #0x0700, %sr
	rts

ss_irq_restore:
	move.l 4(%sp), %d0
	move.w %d0, %sr
	rts

	|      ----------------------------------------------------------------------------
	|      ss_task_yield - voluntary context switch (callable from C).
	|      Build a manual resume frame (return PC + SR), save all registers, mark the
	|      current task as yielded, save its SP, let the C scheduler pick the next task
	|      then resume whatever it chose.
	|      ----------------------------------------------------------------------------
	.globl ss_task_yield

ss_task_yield:
	pea     .yield_resume        | return PC for when we come back
	move.w  %sr, -(sp)           | restore this task's own SR
	movem.l d0-d7/a0-a6, -(sp)   | save ALL regs (resume in another context)

	move.l ss_curr_task, a1
	move.b #1, SS_TCB_RESUME_TYPE_OFFSET(a1) | resume_type = 1 (yielded)
	move.l sp, SS_TCB_CONTEXT_OFFSET(a1) | curr_task->context = SP

	bsr ss_do_context_switch | C scheduler picks ss_scheduled_task
	bra .resume_task

.yield_resume:
	rts

	| ----------------------------------------------------------------------------
	| .resume_task - switch to ss_scheduled_task's saved stack and resume it.
	| Three cases:
	| * new task      (context == stack_base && entry != NULL) -> jump to entry
	| * yielded       (resume_type == 1)      -> pop regs/SR/PC, jmp (no rte)
	| * interrupted   (resume_type == 0)      -> pop regs, rte  [unused here]
	| ----------------------------------------------------------------------------

.resume_task:
	move.l ss_scheduled_task, a1
	move.l SS_TCB_CONTEXT_OFFSET(a1), sp | adopt the target task's stack

	|      new-task test: context == stack_base and entry is valid?
	move.l SS_TCB_CONTEXT_OFFSET(a1), a0
	move.l SS_TCB_STACK_BASE_OFFSET(a1), d0
	cmp.l  a0, d0
	bne.s  .resume_existing
	tst.l  SS_TCB_ENTRY_OFFSET(a1)
	bne.w  .start_task

.resume_existing:

	|      resume_type offset comes from scheduler_abi.h.
	cmpi.b #0, SS_TCB_RESUME_TYPE_OFFSET(a1)
	beq.s  .resume_interrupted

	|       yielded: manual SR/PC restore
	movem.l (sp)+, d0-d7/a0-a6
	move.w  (sp)+, %sr
	move.l  (sp)+, %a0
	jmp     (%a0)

.resume_interrupted:
	|       interrupted (CPU frame intact) — unused in cooperative-only test
	movem.l (sp)+, d0-d7/a0-a6
	rte

.start_task:
	|      first run of a task: call entry(arg), then terminate on return
	move.l SS_TCB_ENTRY_OFFSET(a1), a0 | a0 = task entry function
	move.l SS_TCB_ARG_OFFSET(a1), -(sp)
	move.w #0x2000, %sr
	jsr    (%a0)
	addq.l #SS_FRAME_ARG_BYTES, sp
	jmp    ss_task_exit
