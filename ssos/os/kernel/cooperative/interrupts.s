		.include "iocscall.mac"

		.section .text
		.align	2
		.globl	ss_set_interrupts, ss_restore_interrupts
		.globl	ss_disable_interrupts, ss_enable_interrupts
		.globl	ss_tick_counter, ss_vsync_counter
		.globl	ss_vsync_flag
		.globl	ss_save_data_base
		.globl	ss_context_switch
		.globl	ss_task_yield
		.globl	ss_vdisp_fire_count
		.globl	ss_timerd_fire_count
		.globl	ss_context_switch_count
		.type	ss_set_interrupts, @function

ss_disable_interrupts:
		move.w	#0x2700, %sr
		rts

ss_enable_interrupts:
		move.w	#0x2000, %sr
		rts

		| ============================================================
		| ss_set_interrupts - Initialize MFP and interrupt vectors
		| ============================================================
ss_set_interrupts:
		movem.l	d2-d7/a2-a6, -(sp)

		| Disable interrupts
		move.w	#0x2700, %sr

		| Save original interrupt state
		bsr	save_interrupts

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
		| Save interrupt state for restore
		| ============================================================
	save_interrupts:
		lea		ss_save_data_base, a0

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
		rts

		| ============================================================
		| ss_restore_interrupts - Restore original interrupt state
		| ============================================================
ss_restore_interrupts:
		move.w	#0x2700, %sr

		| Reset pending
		move.b	#0x00, 0xe8800b
		move.b	#0x00, 0xe8800d
		move.b	#0x00, 0xe8800f
		move.b	#0x00, 0xe88011

		lea		ss_save_data_base, a0

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
					move.w	#0x2000, %sr		| Re-enable interrupts
			rte

		| ============================================================
		| TimerD handler - Cooperative: tick counter + wakeups only
		| NO preemptive context switch. Tasks yield exclusively via
		| ss_task_yield().  Since ss_do_wakeups follows the C ABI
		| (preserving callee-saved d2-d7/a2-a6), the ISR only needs
		| to save caller-saved d0-d1/a0-a1 (4 regs, 16 bytes).
		|
		| SSTask struct offsets:
		|   context    = 0   (void*)
		|   prev       = 4   (SSTask*)
		|   next       = 8   (SSTask*)
		|   stack_base = 12  (void*)
		|   stack_size = 16  (uint32_t)
		|   entry      = 20  (function pointer)
		|   wait_until = 24  (uint32_t)
		|   state      = 28  (uint8_t)
		|   pri        = 29  (uint8_t)
		|   ctx_level  = 30  (uint8_t)
		|   pad        = 31  (uint8_t)
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
						move.w	#0x2000, %sr		| Re-enable interrupts
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
		move.l	sp, (a1)

		| Call C scheduler to pick next task
		bsr	ss_do_context_switch

		| Fall through to .resume_task

		| ============================================================
		| .resume_task - Resume the scheduled task
		| Uses resume_type (TCB offset 31) to select restore method
		| ============================================================
	.resume_task:
		move.l	ss_scheduled_task, a1
		move.l	(a1), sp

		| Check if this is a new (never-run) task
		| New task: context was initialized to stack_base in ss_task_create
		move.l	(a1), a0
		move.l	12(a1), d0
		cmp.l	a0, d0
		beq.w	.start_task

		| Check resume_type at TCB offset 31
		cmpi.b	#0, 31(a1)
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
		| New task: set SR and jump directly (no rte)
		move.l	20(a1), a0		| a0 = task entry function
		move.w	#0x2000, %sr		| enable interrupts
		jmp		(%a0)		| start the task

		| ============================================================
		| ss_task_yield - Voluntary context switch (callable from C)
		| ============================================================
ss_task_yield:
		| Build manual resume frame: SR + return PC
		pea		.yield_resume
		move.w	#0x2000, -(sp)
		| Save all registers (must save ALL — cooperative switch resumes
	| in a different task context where d2-d7/a2-a6 are clobbered)
		movem.l	d0-d7/a0-a6, -(sp)
		| Mark as yielded (resume_type = 1)
		move.l	ss_curr_task, a1
		move.b	#1, 31(a1)
		| Save SP and switch
		move.l	sp, (a1)
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

		| ============================================================
		| TRAP #14 Exception Handler
		|
		| Human68K issues TRAP #14 when a bus error, address error,
		| or illegal instruction occurs.  The CPU pushes SR+PC for
		| the trap, so the handler MUST return with rte.
		|
		| On entry (set by Human68K before TRAP #14):
		|   d7 = exception code (2=bus err, 3=addr err, 4=illegal inst)
		|   a6 = pointer to parameter block (SR at +0, PC at +2)
		|
		| Design (per the X68000 programming reference):
		|   1. Handler saves exception info to trapbuf (NO I/O here!)
		|   2. Handler issues _ABORTJOB (do=-1) so Human68K invokes
		|      the process abort vectors (0xFFF2 / 0xFFF1).
		|   3. The abort handler (ss_trap14_abort) prints info and exits.
		|   4. For exceptions we don't handle: chain to original handler.
		| ============================================================

		| --- trapbuf data (C-accessible via .globl) ---
		.section .data
		.even
		.globl	ss_old_trap14
		.globl	ss_trapbuf_flag
		.globl	ss_trapbuf_sr
		.globl	ss_trapbuf_pc
		.globl	ss_trapbuf_msg

ss_old_trap14:
		dc.l	0
ss_trapbuf_flag:
		dc.w	0
ss_trapbuf_sr:
		dc.w	0
ss_trapbuf_pc:
		dc.l	0
ss_trapbuf_msg:
		dc.l	0

		| --- String constants ---
		.section .rodata
		.even
ss_msg_bus:
		.ascii	"Bus Error\0"
ss_msg_addr:
		.ascii	"Address Error\0"
ss_msg_ill:
		.ascii	"Illegal Instruction\0"
ss_msg_banner:
		.ascii	"\r\n[TRAP14] \0"
ss_msg_pc:
		.ascii	" PC=\0"
ss_msg_crlf:
		.ascii	"\r\n\0"

		.section .data
		.even
ss_hex_buf:
		.ds.b	16

		| --- TRAP #14 code ---
		.section .text
		.even
		.globl	ss_trap14_handler
		.globl	ss_trap14_abort
		.globl	ss_init_trap14
		.globl	ss_restore_trap14

		| ----------------------------------------------------------
		| print_hex_long — print d0 as 8-digit hex via _B_PRINT
		| Clobbers: d0-d2, a0-a1
		| ----------------------------------------------------------
	print_hex_long:
		movem.l	d2/a0-a1, -(sp)
		lea		ss_hex_buf, a0
		move.l	d0, d2
		moveq	#7, d1
	.hex_loop:
		rol.l	#4, d2
		move.w	d2, d0
		andi.w	#0x0F, d0
		cmpi.w	#10, d0
		blt.s	.hex_digit
		addi.w	#'A'-10, d0
		bra.s	.hex_store
	.hex_digit:
		addi.w	#'0', d0
	.hex_store:
		move.b	d0, (a0)+
		dbra	d1, .hex_loop
		clr.b	(a0)
		IOCS	_B_PRINT
		movem.l	(sp)+, d2/a0-a1
		rts

		| ----------------------------------------------------------
		| TRAP #14 handler entry point
		| ----------------------------------------------------------
ss_trap14_handler:
		| Save working registers (NOT d7/a6 — they hold params)
		movem.l	d0-d1/a0-a1, -(sp)

		| Read exception code from d7 (set by Human68K)
		move.w	d7, d0
		andi.w	#0xFFFF, d0

		| Decode exception type
		cmpi.w	#2, d0		| Bus Error
		beq.s	.handle
		cmpi.w	#3, d0		| Address Error
		beq.s	.handle
		cmpi.w	#4, d0		| Illegal Instruction
		beq.s	.handle
		bra.w	.chain_to_old		| Other: chain to original handler

	.handle:
		| Save exception code
		move.w	d7, ss_trapbuf_flag

		| Select message pointer
		cmpi.w	#2, d0
		bne.s	.not_bus
		lea		ss_msg_bus, a0
		bra.s	.save_msg
	.not_bus:
		cmpi.w	#3, d0
		bne.s	.not_addr
		lea		ss_msg_addr, a0
		bra.s	.save_msg
	.not_addr:
		lea		ss_msg_ill, a0
	.save_msg:
		move.l	a0, ss_trapbuf_msg

		| Extract SR and PC from parameter block at a6
		move.l	a6, d0
		btst	#0, d0
		bne.s	.a6_unaligned

		move.w	(a6), ss_trapbuf_sr
		move.l	2(a6), ss_trapbuf_pc
		bra.s	.do_abort

	.a6_unaligned:
		move.w	#0xFFFF, ss_trapbuf_sr
		move.l	a6, ss_trapbuf_pc

	.do_abort:
		| Restore working registers before abort
		movem.l	(sp)+, d0-d1/a0-a1

		| Issue _ABORTJOB (do=-1) — this tells Human68K to
		| invoke the process abort vectors (0xFFF2 / 0xFFF1)
		| where we installed ss_trap14_abort.
		IOCS	_ABORTJOB

		| If abort somehow returns, rte back to Human68K
		rte

	.chain_to_old:
		| Restore working registers
		movem.l	(sp)+, d0-d1/a0-a1
		| Jump to original handler — it will do the rte
		move.l	ss_old_trap14, a0
		jmp		(a0)

		| ----------------------------------------------------------
		| ss_trap14_abort — called by Human68K after _ABORTJOB
		|
		| Installed at process vectors 0xFFF2 (error abort) and
		| 0xFFF1 (Ctrl-C abort) via _B_INTVCS.
		| Must NOT return — _ABORTRST or exit() is required.
		| ----------------------------------------------------------
ss_trap14_abort:
		| Restore original TRAP #14 vector so subsequent errors
		| go to the default handler ("white window")
		move.l	ss_old_trap14, d0
		move.l	d0, 0xB8

		| Print exception info via _B_PRINT (IOCS 0x21)
		lea		ss_msg_banner, a1
		IOCS	_B_PRINT

		move.l	ss_trapbuf_msg, a1
		IOCS	_B_PRINT

		lea		ss_msg_pc, a1
		IOCS	_B_PRINT

		move.l	ss_trapbuf_pc, d0
		bsr		print_hex_long

		lea		ss_msg_crlf, a1
		IOCS	_B_PRINT

		| Exit — _ABORTRST (IOCS 0xFD) terminates the process
		IOCS	_ABORTRST

		| ----------------------------------------------------------
		| ss_init_trap14 — install TRAP #14 handler + abort vectors
		| ----------------------------------------------------------
ss_init_trap14:
		move.w	#0x2700, %sr

		| Save original TRAP #14 vector (0xB8 = vector 0x2e * 4)
		move.l	0xB8, ss_old_trap14

		| Install our TRAP #14 handler at 0xB8
		lea		ss_trap14_handler, a0
		move.l	a0, 0xB8

		| Clear trapbuf
		clr.w	ss_trapbuf_flag
		clr.w	ss_trapbuf_sr
		clr.l	ss_trapbuf_pc

		| Install abort handler at process vectors via _B_INTVCS
		| Vector 0xFFF2 = error abort, 0xFFF1 = ctrl-C abort
		lea		ss_trap14_abort, a0
		move.w	#0xFFF2, d1
		IOCS	_B_INTVCS
		lea		ss_trap14_abort, a0
		move.w	#0xFFF1, d1
		IOCS	_B_INTVCS

		move.w	#0x2000, %sr
		rts

		| ----------------------------------------------------------
		| ss_restore_trap14 — restore original TRAP #14 handler
		| ----------------------------------------------------------
ss_restore_trap14:
		move.w	#0x2700, %sr

		| Restore original TRAP #14 vector
		move.l	ss_old_trap14, d0
		move.l	d0, 0xB8

		move.w	#0x2000, %sr
		rts

		.end interrupts
