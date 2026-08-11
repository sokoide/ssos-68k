	.include "iocscall.mac"

	.section .text
	.align	2
	.globl	entry
	.type	entry, @function

entry:
	| Set supervisor stack pointer
	move.l	#0x010000, a0
	move.l	a0, sp
	clr.l	-(sp)

	| Jump to C initialization
	jmp	premain

	.end entry
