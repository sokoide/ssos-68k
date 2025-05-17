	.include "iocscall.mac"

	.section .text
	.align	2
	.globl	entry
	.type	entry, @function

entry:
	# ssp
	move.l	#0x010000, a0
	move.l	a0, sp
	clr.l	-(sp)

	# usp
	/* move.l	#0x280000, a0 */
	/* move.l	a0, %usp */

	bsr set_interrupts

	jmp		premain

	.end entry
