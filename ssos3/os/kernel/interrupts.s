	.include "iocscall.mac"

	.section .text
	.align	2
	.globl	s3_set_interrupts, s3_restore_interrupts
	.globl	s3_disable_interrupts, s3_enable_interrupts
	.globl	s3_tick_counter, s3_vsync_counter
	.globl	s3_save_data_base
	.type	s3_set_interrupts, @function

s3_disable_interrupts:
	move.w	#0x2700, %sr
	rts

s3_enable_interrupts:
	move.w	#0x2000, %sr
	rts

	| ============================================================
	| s3_set_interrupts - Initialize MFP and interrupt vectors
	| ============================================================
s3_set_interrupts:
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
	lea		s3_timerd_handler, a0
	move.l	a0, 0x110

	| V-DISP / Timer A handler
	lea		s3_vdisp_handler, a0
	move.l	a0, 0x134

	| NOP handlers for unused vectors
	lea		s3_nop_handler, a0
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

	lea		s3_save_data_base, a0

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
	| s3_restore_interrupts - Restore original interrupt state
	| ============================================================
s3_restore_interrupts:
	movem.l	d2-d7/a2-a6, -(sp)

	move.w	#0x2700, %sr

	| Reset pending
	move.b	#0x00, 0xe8800b
	move.b	#0x00, 0xe8800d
	move.b	#0x00, 0xe8800f
	move.b	#0x00, 0xe88011

	lea		s3_save_data_base, a0

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
	| Interrupt handlers - minimal path
	| ============================================================
s3_nop_handler:
	rte

	| V-DISP handler: increment vsync counter and reset ISR
s3_vdisp_handler:
	movem.l	d0/a0, -(sp)

	| Reset ISRA Timer A bit
	move.l	#0xe8800f, a0
	move.b	(a0), d0
	and.b	#0xdf, d0
	move.b	d0, (a0)

	addq.l	#1, s3_vsync_counter

	movem.l	(sp)+, d0/a0
	rte

	| ============================================================
	| TimerD handler - 1ms tick, minimal register save
	| ============================================================
	.extern s3_curr_task
	.extern s3_scheduled_task
	.extern s3_do_context_switch

s3_timerd_handler:
	| Save minimal registers: d0-d1/a0-a1 (16 bytes)
	movem.l	d0-d1/a0-a1, -(sp)

	| Increment tick counter
	addq.l	#1, s3_tick_counter

	| Context switch every 5ms (every 5th tick)
	lea		s3_switch_tick, a0
	addq.b	#1, (a0)
	cmpi.b	#5, (a0)
	bne.s	.no_switch

	move.b	#0, (a0)

	| Check if there's a current task to switch from
	move.l	s3_curr_task, d0
	beq.s	.no_switch

	| Call C context switch handler
	bsr	s3_do_context_switch

.no_switch:
	| Reset ISRB Timer D bit
	move.l	#0xe88011, a0
	move.b	(a0), d0
	andi.b	#0xef, d0
	move.b	d0, (a0)

	movem.l	(sp)+, d0-d1/a0-a1
	rte

	| ============================================================
	| Data section
	| ============================================================
	.section .data
	.even
s3_tick_counter:
	dc.l	0
s3_vsync_counter:
	dc.l	0
s3_switch_tick:
	dc.b	0
	.even

	.section .bss
	.even
s3_save_data_base:
	ds.b	1024

	.end interrupts
