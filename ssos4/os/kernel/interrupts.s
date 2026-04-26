		.include "iocscall.mac"

		.section .text
		.align	2
		.globl	s4_set_interrupts, s4_restore_interrupts
		.globl	s4_disable_interrupts, s4_enable_interrupts
		.globl	s4_tick_counter, s4_vsync_counter
		.globl	s4_save_data_base
		.type	s4_set_interrupts, @function

s4_disable_interrupts:
		move.w	#0x2700, %sr
		rts

s4_enable_interrupts:
		move.w	#0x2000, %sr
		rts

		| ============================================================
		| s4_set_interrupts - Initialize MFP and interrupt vectors
		| ============================================================
s4_set_interrupts:
		movem.l	d2-d7/a2-a6, -(sp)

		| Disable interrupts
		move.w	#0x2700, %sr

		| Save original interrupt state
		bsr	save_interrupts

		| Set MFP vector base
		move.b	#0x41, 0xe88017

		| IERAB: enable all
		move.b	#0xff, 0xe88007
		move.b	#0x7f, 0xe88009

		| IMRAB: mask all initially
		move.b	#0xff, 0xe88013
		move.b	#0x7f, 0xe88015

		| Reset IPRAB, ISRAB
		move.b	#0x00, 0xe8800b
		move.b	#0x00, 0xe8800d
		move.b	#0x00, 0xe8800f
		move.b	#0x00, 0xe88011

		| Set interrupt handlers
		| Timer D handler
		lea		s4_timerd_handler, a0
		move.l	a0, 0x110

		| V-DISP / Timer A handler
		lea		s4_vdisp_handler, a0
		move.l	a0, 0x134

		| NOP handlers for unused vectors
		lea		s4_nop_handler, a0
		move.l	a0, 0x138
		move.l	a0, 0x13c

		| TACR - event count mode (V-DISP)
		move.b	#0x08, 0xe88019

		| TCDCR - Timer D prescaler /50
		move.b	0xe8801d, d0
		or.b	#0xF4, d0
		move.b	d0, 0xe8801d

		| TADR - Timer A data
		move.b	#1, 0xe8801f

		| TDDR - Timer D: 4MHz / 50 / 80 = 1000Hz (1ms)
		move.b	#80, 0xe88025

		| Enable interrupts - level 2
		move.w	#0x2000, %sr

		movem.l	(sp)+, d2-d7/a2-a6
		rts

		| ============================================================
		| Save interrupt state for restore
		| ============================================================
save_interrupts:
		movem.l	d2-d7/a2-a6, -(sp)

		lea		s4_save_data_base, a0

		| Timer A
		move.l	0x134, d0
		move.l	d0, (a0)
		| Timer D
		move.l	0x110, d0
		move.l	d0, 4(a0)
		| Key
		move.l	0x130, d0
		move.l	d0, 8(a0)

		| Save MFP registers
		lea		0xe88007, a1
		move.b	(a1), d0
		move.b	d0, 12(a0)
		lea		0xe88009, a1
		move.b	(a1), d0
		move.b	d0, 13(a0)
		lea		0xe88013, a1
		move.b	(a1), d0
		move.b	d0, 14(a0)
		lea		0xe88015, a1
		move.b	(a1), d0
		move.b	d0, 15(a0)

		| Timer registers
		lea		0xe88019, a1
		move.b	(a1), d0
		move.b	d0, 16(a0)
		lea		0xe8801d, a1
		move.b	(a1), d0
		move.b	d0, 17(a0)
		lea		0xe8801f, a1
		move.b	(a1), d0
		move.b	d0, 18(a0)
		lea		0xe88025, a1
		move.b	(a1), d0
		move.b	d0, 19(a0)
		lea		0xe88017, a1
		move.b	(a1), d0
		move.b	d0, 20(a0)

		| CRTC vectors
		move.l	0x138, d0
		move.l	d0, 24(a0)
		move.l	0x13c, d0
		move.l	d0, 28(a0)

		move.w	#0x2700, %sr
		movem.l	(sp)+, d2-d7/a2-a6
		rts

		| ============================================================
		| s4_restore_interrupts - Restore original interrupt state
		| ============================================================
s4_restore_interrupts:
		movem.l	d2-d7/a2-a6, -(sp)

		move.w	#0x2700, %sr

		| Reset pending
		move.b	#0x00, 0xe8800b
		move.b	#0x00, 0xe8800d
		move.b	#0x00, 0xe8800f
		move.b	#0x00, 0xe88011

		lea		s4_save_data_base, a0

		| Restore vectors
		move.l	(a0), d0
		move.l	d0, 0x134
		move.l	4(a0), d0
		move.l	d0, 0x110
		move.l	8(a0), d0
		move.l	d0, 0x130

		| Restore MFP registers
		move.b	12(a0), d0
		move.b	d0, 0xe88007
		move.b	13(a0), d0
		move.b	d0, 0xe88009
		move.b	14(a0), d0
		move.b	d0, 0xe88013
		move.b	15(a0), d0
		move.b	d0, 0xe88015

		| Timer registers
		move.b	16(a0), d0
		move.b	d0, 0xe88019
		move.b	17(a0), d0
		move.b	d0, 0xe8801d
		move.b	18(a0), d0
		move.b	d0, 0xe8801f
		move.b	19(a0), d0
		move.b	d0, 0xe88025
		move.b	20(a0), d0
		move.b	d0, 0xe88017

		| CRTC vectors
		move.l	24(a0), d0
		move.l	d0, 0x138
		move.l	28(a0), d0
		move.l	d0, 0x13c

		move.w	#0x2700, %sr
		movem.l	(sp)+, d2-d7/a2-a6
		rts

		| ============================================================
		| Interrupt handlers
		| ============================================================
s4_nop_handler:
		rte

		| V-DISP handler: increment vsync counter and reset ISR
s4_vdisp_handler:
		movem.l	d0/a0, -(sp)

		| Reset ISRA Timer A bit
		move.l	#0xe8800f, a0
		move.b	(a0), d0
		and.b	#0xdf, d0
		move.b	d0, (a0)

		addq.l	#1, s4_vsync_counter

		movem.l	(sp)+, d0/a0
		rte

		| ============================================================
		| TimerD handler - 1ms tick with preemptive context switch
		|
		| S4Task struct offsets:
		|   context    = 0   (void*)
		|   prev       = 4   (S4Task*)
		|   next       = 8   (S4Task*)
		|   stack_base = 12  (void*)
		|   stack_size = 16  (uint32_t)
		|   entry      = 20  (function pointer)
		|   wait_until = 24  (uint32_t)
		|   state      = 28  (uint8_t)
		| ============================================================
		.extern s4_curr_task
		.extern s4_scheduled_task
		.extern s4_do_context_switch

s4_timerd_handler:
		| Save ALL registers for preemptive context switch
		| 15 registers: d0-d7 + a0-a6 = 60 bytes
		movem.l	d0-d7/a0-a6, -(sp)

		| Increment tick counter
		addq.l	#1, s4_tick_counter

		| Context switch every 5ms (every 5th tick)
		lea		s4_switch_tick, a0
		addq.b	#1, (a0)
		cmpi.b	#5, (a0)
		bne.s	.no_switch

		move.b	#0, (a0)

		| Check if there's a current task
		move.l	s4_curr_task, d0
		beq.s	.no_switch

		| Reset MFP Timer D ISR bit BEFORE switching stacks
		move.l	#0xe88011, a0
		move.b	(a0), d1
		andi.b	#0xef, d1
		move.b	d1, (a0)

		| Jump to context switch (does not return here)
		bra.w	s4_context_switch

	.no_switch:
		| Reset ISRB Timer D bit
		move.l	#0xe88011, a0
		move.b	(a0), d0
		andi.b	#0xef, d0
		move.b	d0, (a0)

		movem.l	(sp)+, d0-d7/a0-a6
		rte

		| ============================================================
		| s4_context_switch - Preemptive context switch
		|
		| Called from timer handler. Never returns to caller.
		| On entry: all registers saved on current task's stack
		|           by timer handler's movem.l
		| ============================================================
s4_context_switch:
		| Save current SP to curr_task->context
		move.l	s4_curr_task, a1
		move.l	sp, (a1)

		| Call C scheduler to pick next task
		bsr	s4_do_context_switch

		| Load next task's context SP
		move.l	s4_scheduled_task, a1
		move.l	(a1), sp

		| Check if this is a new (never-run) task
		| New task: context was initialized to stack_base in s4_task_create
		move.l	(a1), a0
		move.l	12(a1), d0
		cmp.l	a0, d0
		beq.w	.start_new_task

		| Existing task: restore registers and return from exception
		movem.l	(sp)+, d0-d7/a0-a6
		rte

	.start_new_task:
		| Build initial exception frame for new task
		| rte pops: SR (2 bytes), then PC (4 bytes)
		move.l	20(a1), a0		| a0 = task entry function
		move.l	12(a1), sp		| sp = stack_base (top of stack)
		move.l	a0, -(sp)		| Push PC (task entry point)
		move.w	#0x2000, -(sp)		| Push SR (supervisor, interrupts enabled)
		rte				| Start the new task

		| ============================================================
		| Data section
		| ============================================================
		.section .data
		.even
s4_tick_counter:
		dc.l	0
s4_vsync_counter:
		dc.l	0
s4_switch_tick:
		dc.b	0
		.even

		.section .bss
		.even
s4_save_data_base:
		ds.b	1024

		.end interrupts
