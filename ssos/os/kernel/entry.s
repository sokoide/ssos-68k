	.section .text
	.align	2
	.globl	entry
	.type	entry, @function

entry:
	lea		0x03000000, %sp
	jmp		main

	.end entry
