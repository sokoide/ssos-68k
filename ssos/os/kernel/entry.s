	.include "iocscall.mac"

	.section .text
	.align	2
	.globl	entry
	.type	entry, @function

entry:
    # set stack = 64KB - 0x400 (interrupt vector table)
	move.l	#0x010000, %a0
	move.l	%a0, %sp
	clr.l	-(%sp)

	bsr interrupts

	/* move.l	#0x280000, %a0 */
	/* move.l	%a0, %usp */

	# it's already in the supervisor mode
	/* move.l	#0, %a1 */
	/* IOCS	_B_SUPER */


	jmp		premain

loop:
	jmp		loop

	.end entry

