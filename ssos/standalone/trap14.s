		.include "iocscall.mac"

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
ss_msg_abort:
		.ascii	"Process Abort\0"
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
		| The interrupted task stack may be corrupt. Use an emergency stack
		| before calling C or IOCS and never return to the interrupted task.
		lea		ss_abort_stack_end, sp
		jsr		ss_abort_cleanup
		| Restore original TRAP #14 vector so subsequent errors
		| go to the default handler ("white window")
		move.l	ss_old_trap14, d0
		move.l	d0, 0xB8

		| Print exception info via _B_PRINT (IOCS 0x21)
		lea		ss_msg_banner, a1
		IOCS	_B_PRINT

		move.l	ss_trapbuf_msg, a1
		move.l	a1, d0
		bne.s	.have_message
		lea		ss_msg_abort, a1
	.have_message:
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

		.section .bss
		.even
ss_abort_stack:
		.ds.b	8192
ss_abort_stack_end:
