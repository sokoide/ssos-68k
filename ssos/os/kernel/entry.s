	.section .text
	.align	2
	.globl	entry
	.type	entry, @function

entry:
    # set stack
	move.l	#0x300000, %sp
	jmp		main

	.end entry
