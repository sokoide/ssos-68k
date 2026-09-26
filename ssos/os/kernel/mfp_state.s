		.section .text
		.even
		.globl save_interrupts, ss_restore_interrupts
		.globl ss_save_data_base

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
