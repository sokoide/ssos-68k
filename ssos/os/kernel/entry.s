	.include "iocscall.mac"

	.section .text
	.align	2
	.globl	entry
	.type	entry, @function

entry:
	bsr setstack
	bsr interrupts

	jmp		premain

setstack:
	# ssp
	move.l	#0x010000, %a0
	move.l	%a0, %sp
	clr.l	-(%sp)

	# usp
	/* move.l	#0x280000, %a0 */
	/* move.l	%a0, %usp */

	rts

	.end entry

