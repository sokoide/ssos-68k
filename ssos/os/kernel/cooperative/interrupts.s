		.include "iocscall.mac"
		#include "scheduler_abi.h"

		.section .text
		.align	2
		.globl	ss_set_interrupts, ss_restore_interrupts
		.globl	ss_irq_save, ss_irq_restore
		.globl	ss_tick_counter, ss_vsync_counter
		.globl	ss_vsync_flag
		.globl	ss_save_data_base
		.globl	ss_context_switch
		.globl	ss_task_yield
		.globl	ss_vdisp_fire_count
		.globl	ss_timerd_fire_count
		.globl	ss_context_switch_count
		.type	ss_set_interrupts, @function

ss_irq_save:
		move.w	%sr, %d0
		ori.w	#0x0700, %sr
		rts

ss_irq_restore:
		move.l	4(%sp), %d0
		move.w	%d0, %sr
		rts

		| ============================================================
		| ss_set_interrupts - Initialize MFP and interrupt vectors
		| ============================================================
ss_set_interrupts:
		movem.l	d2-d7/a2-a6, -(sp)

		| Disable interrupts
		move.w	#0x2700, %sr

		| Save original interrupt state
		jsr	save_interrupts

		| Set MFP vector base
		move.b	#0x41, 0xe88017

		| IERAB: keep all sources enabled (Human68k-compatible).
		|   IER bits may be needed by IOCS (USART, Timers) even when
		|   the corresponding interrupt is masked.
		move.b	#0xff, 0xe88007
		move.b	#0x7f, 0xe88009

		| IMRAB: keep all sources unmasked so Human68K's handlers
		|   (keyboard, mouse, CRTC, etc.) continue to work.
		|   Our code overrides only specific vectors (0x134 V-DISP,
		|   0x110 Timer D); all others still point to Human68K ISRs.
		|   IMRB bit 7 is reserved and must stay masked (0x7F).
		move.b	#0xff, 0xe88013
		move.b	#0x7f, 0xe88015

		| Reset IPRAB, ISRAB
		move.b	#0x00, 0xe8800b
		move.b	#0x00, 0xe8800d
		move.b	#0x00, 0xe8800f
		move.b	#0x00, 0xe88011

		| Set interrupt handlers
		| Timer D handler
		lea		ss_timerd_handler, a0
		move.l	a0, 0x110

		| V-DISP / Timer A handler
		lea		ss_vdisp_handler, a0
		move.l	a0, 0x134

		| NOP handlers for unused vectors
		lea		ss_nop_handler, a0
		move.l	a0, 0x138
		move.l	a0, 0x13c

		| TACR - event count mode (V-DISP)
		|   $08 = event count mode (Human68k compatible)
		move.b	#0x08, 0xe88019

		| TCDCR - Timer C/D prescaler /200 each (delay mode)
		|   $77 = 0111 0111 → C:/200, D:/200
		|   Use move.b (not read-modify-write) to avoid setting
		|   reserved bit 7 which ORing 0xF7 would do.
		move.b	#0x77, 0xe8801d

		| TADR - Timer A data
		move.b	#1, 0xe8801f

		| TDDR - Timer D: 4MHz / 200 / 100 = 200Hz (5ms)
		move.b	#100, 0xe88025

		| Enable interrupts - level 2
		move.w	#0x2000, %sr

		movem.l	(sp)+, d2-d7/a2-a6
		rts


		| ============================================================
		| Interrupt handlers
		| ============================================================
		.globl	ss_nop_handler
		.globl	ss_vdisp_handler
		.globl	ss_timerd_handler
ss_nop_handler:
		rte

		| V-DISP handler: increment vsync counter and set flag
ss_vdisp_handler:
			move.w	#0x2700, %sr		| Disable interrupts to prevent nesting
		movem.l	d0/a0, -(sp)

		| Reset ISRA Timer A bit (clear bit 5)
		move.l	#0xe8800f, a0
		move.b	(a0), d0
		and.b	#0xdf, d0
		move.b	d0, (a0)

		addq.l	#1, ss_vsync_counter
		move.b	#1, ss_vsync_flag
		addq.l	#1, ss_vdisp_fire_count

		movem.l	(sp)+, d0/a0
			rte

		| ============================================================
		| TimerD handler - Cooperative: tick counter + wakeups only
		| NO preemptive context switch. Tasks yield exclusively via
		| ss_task_yield(). Wakeups run later at the scheduler boundary.
		| This ISR saves only d0/a0 for its counters and MFP EOI.
		|
		| TCB offsets shared with the C layout in scheduler_abi.h.
		| ============================================================

			.globl	ss_wakeups_needed
ss_timerd_handler:
			move.w	#0x2700, %sr		| Disable interrupts to prevent nesting
		| Minimal save: only d0/a0 for flag set and counter increment.
		movem.l	d0/a0, -(sp)
		addq.l	#1, ss_tick_counter
		addq.l	#1, ss_timerd_fire_count

		| Reset ISRB Timer D bit (clear bit 4)
			| Wakeups処理が必要ことを示すフラグを設定
			move.b	#1, ss_wakeups_needed
		move.l	#0xe88011, a0
		move.b	(a0), d0
		andi.b	#0xef, d0
		move.b	d0, (a0)

		movem.l	(sp)+, d0/a0
			rte

		| ============================================================
		| ss_context_switch - Save current task, switch to next
		|
		| On entry: all registers saved on current task's stack
		|           resume_type already set in TCB
		| ============================================================
ss_context_switch:
		| Save current SP to curr_task->context
		move.l	ss_curr_task, a1
		move.l	sp, SS_TCB_CONTEXT_OFFSET(a1)

		| Call C scheduler to pick next task
		bsr	ss_do_context_switch

		| Fall through to .resume_task

		| ============================================================
		| .resume_task - Resume the scheduled task
		| Uses the shared resume_type offset to select restore method
		| ============================================================
	.resume_task:
		move.l	ss_scheduled_task, a1
		move.l	SS_TCB_CONTEXT_OFFSET(a1), sp

		| Check if this is a new (never-run) task.  A created task has
		| context == stack_base and a non-NULL entry.  The bootstrap main
		| task uses the same stack sentinel but deliberately has no entry.
		move.l	SS_TCB_CONTEXT_OFFSET(a1), a0
		move.l	SS_TCB_STACK_BASE_OFFSET(a1), d0
		cmp.l	a0, d0
		bne.s	.resume_existing
		tst.l	SS_TCB_ENTRY_OFFSET(a1)
		bne.w	.start_task
	.resume_existing:

		| Check resume_type at its shared TCB offset
		cmpi.b	#0, SS_TCB_RESUME_TYPE_OFFSET(a1)
		beq.s	.resume_interrupted

		| resume_type == 1: yielded, manual SR/PC restore (no rte)
		movem.l	(sp)+, d0-d7/a0-a6
		move.w	(sp)+, %sr
		move.l	(sp)+, %a0
		jmp		(%a0)

	.resume_interrupted:
		| resume_type == 0: timer-interrupted, CPU frame intact, rte safe
		movem.l	(sp)+, d0-d7/a0-a6
		rte

	.start_task:
		| New task: call the C entry with arg; returning ends the task.
		move.l	SS_TCB_ENTRY_OFFSET(a1), a0		| a0 = task entry function
		move.l	SS_TCB_ARG_OFFSET(a1), -(sp)		| entry(arg)
		move.w	#0x2000, %sr		| enable interrupts
		jsr		(%a0)
		addq.l	#SS_FRAME_ARG_BYTES, sp
		jmp		ss_task_exit

		| ============================================================
		| ss_task_yield - Voluntary context switch (callable from C)
		| ============================================================
ss_task_yield:
		| Build manual resume frame: SR + return PC.
		| Unlike the preemptive yield, no IPL7 mask is needed before writing
		| resume_type/context: the cooperative Timer D ISR never switches
		| context (it only sets the ss_wakeups_needed flag byte), and queue
		| manipulation below runs inside ss_do_context_switch's own mask.
		| Revisit this invariant before adding any queue access to the
		| cooperative ISR.
		pea		.yield_resume
		move.w	%sr, -(sp)
		| Save all registers (must save ALL — cooperative switch resumes
	| in a different task context where d2-d7/a2-a6 are clobbered)
		movem.l	d0-d7/a0-a6, -(sp)
		| Mark as yielded (resume_type = 1)
		move.l	ss_curr_task, a1
		move.b	#1, SS_TCB_RESUME_TYPE_OFFSET(a1)
		| Save SP and switch
		move.l	sp, SS_TCB_CONTEXT_OFFSET(a1)
		bsr	ss_do_context_switch
		bra.w	.resume_task
	.yield_resume:
		rts

		| ============================================================
		| Data section
		| ============================================================
		.section .data
		.even
ss_tick_counter:
		dc.l	0
ss_vsync_counter:
		dc.l	0
ss_vsync_flag:
		dc.b	0
		.even
ss_wakeups_needed:
		dc.b	0
		.even
ss_vdisp_fire_count:
		dc.l	0
ss_timerd_fire_count:
		dc.l	0
ss_context_switch_count:
		dc.l	0
		.even

		.section .bss
		.even
ss_save_data_base:
		ds.b	1024
