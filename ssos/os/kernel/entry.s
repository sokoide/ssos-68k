	.include "iocscall.mac"

	.section .text
	.align	2
	.globl	entry
	.type	entry, @function

entry:
    # set stack
	move.l	#0x200000, %a0
	move.l	%a0, %sp
	clr.l	-(%sp)

	move.l	#0x280000, %a0
	move.l	%a0, %usp

	/* move.l	#0, %a1 */
	/* IOCS	_B_SUPER */


	# Disable interrupts - level 7
	move.w	#0x2700, %sr

	# MFP
	# set MFP vector base
	/* move.b  #0x40, 0xe88017 */

	# AER, DDR
	/* move.b	#0x06, 0xe88003 */
	/* move.b	#0x00, 0xe88005 */

	# IERAB
	/* move.b	#0x00, 0xe88007 */
	/* move.b	#0x00, 0xe88009 */

	# reset IPRAB, ISRAB
	/* move.b	#0x00, 0xe88009 */
	/* move.b	#0x00, 0xe8800b */
	/* move.b	#0x00, 0xe8800d */
	/* move.b	#0x00, 0xe8800f */
	/* move.b	#0x00, 0xe88011 */

	# set IMRAB
	/* move.b	#0x18, 0xe88013 */
	/* move.b	#0x3e, 0xe88015 */

	# Timer A, B reset
    /* move.b  #0x08, 0xe88019 */
    /* move.b  #0x01, 0xe8801b */
	/* move.b  #0x10, 0xE8801B */
    /* move.b  #0x10, 0xE8800B */

	# set interrupt handlers
	lea     nop_handler, %a0
	/* move.l  %a0, 0x100 */
	/* move.l  %a0, 0x104 */
	/* move.l  %a0, 0x10c */
	# Timer D
	/* move.l  %a0, 0x110 */
	# Timer C
	/* move.l  %a0, 0x114 */
	# V-DISP state
	/* move.l  %a0, 0x118 */
	# 11c: not used
	/* move.l  %a0, 0x11c */
	/* move.l  %a0, 0x120 */
	/* move.l  %a0, 0x124 */
	# key send
	/* move.l  %a0, 0x128 */
	/* move.l  %a0, 0x12c */
	# key receive
	/* move.l  %a0, 0x130 */
	# CRTC V-DISP
	/* move.l  %a0, 0x134 */
	# CRTC IRQ
	/* move.l  %a0, 0x138 */
	# CRTC H-SYNC
	/* move.l  %a0, 0x13c */

	# TACR
	/* move.b  #0x08, 0xe88019 */
	# TBCR
	/* move.b  #0x01, 0xe8801b */
	# TCDCR
	/* move.b	#0x00, 0xe8801d */

	/* move.b	#0x03, 0xe88021 */
	/* move.b	#0x4d, 0xe88023 */

	# IERAB
	/* move.b	#0x21, 0xe88007 */
	/* move.b	#0x70, 0xe88009 */

	# Enable interrupts - level 2
	move.w	#0x2000, %sr

	jmp		premain

nop_handler:
    rte

	.end entry
