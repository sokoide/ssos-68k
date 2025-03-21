	.include "iocscall.mac"

	.section .text
	.align	2
	.global	set_interrupts, restore_interrupts
	.global ss_timera_counter, ss_timerb_counter, ss_timerc_counter, ss_timerd_counter, ss_key_counter
	.global ss_save_data_base
	.type	set_interrupts, @function
	.type	save_interrupts, @function
	.type	restore_interrupts, @function

set_interrupts:
	movem.l %d2-%d7/%a2-%a6, -(%sp)

	# reset global vars in for timers
	lea		ss_save_data_base, %a0
	move.w	#256, %d0
	clr.l	%d1
clear_loop:
	move.l	%d1, (%a0)+
	dbra	%d0, clear_loop


	# Disable interrupts - level 7
	move.w	#0x2700, %sr

	bsr		save_interrupts

	# MFP
	# set MFP vector base (0x40 is the default)
	move.b  #0x40, 0xe88017

	# AER, DDR
	/* move.b	#0x06, 0xe88003 */
	/* move.b	#0x00, 0xe88005 */

	# IERAB
	move.b	#0xff, 0xe88007
	move.b	#0x7f, 0xe88009

	# set IMRAB masks
	move.b	#0xff, 0xe88013
	move.b	#0x7f, 0xe88015

	# reset IPRAB, ISRAB
	move.b	#0x00, 0xe8800b
	move.b	#0x00, 0xe8800d
	move.b	#0x00, 0xe8800f
	move.b	#0x00, 0xe88011

	# set interrupt handlers
	# Alarm
	/* move.l  %a0, 0x100 */
	# EXPWON
	/* move.l  %a0, 0x104 */
	# POWER SW
	/* move.l  %a0, 0x108 */
	# FM
	/* move.l  %a0, 0x10c */
	# Timer D
	lea		timerd_handler, %a0
	move.l  %a0, 0x110

	# Timer C
	# Used by IOCS's mouse handler, too
	lea		timerc_handler, %a0
	move.l  %a0, 0x114

	# V-DISP state
	/* move.l  %a0, 0x118 */

	# 11c: not used by X68000
	/* lea     nop_handler, %a0 */
	/* move.l  %a0, 0x11c */

	# USART, Timer B, don't change, used by keyboard
	/* lea		usart_handler, %a0 */
	/* move.l  %a0, 0x120 */
	# key send error
	/* lea     nop_handler, %a0 */
	/* move.l  %a0, 0x124 */
	# key send
	/* lea     nop_handler, %a0 */
	/* move.l  %a0, 0x128 */
	/* # key receive error */
	/* lea     nop_handler, %a0 */
	/* move.l  %a0, 0x12c */
	# key receive - if you use IOCS key*, don't override this
	/* lea		key_input_handler, %a0 */
	/* move.l  %a0, 0x130 */
	# CRTC V-DISP, Timer A
	lea		vdisp_handler, %a0
	move.l  %a0, 0x134
	# CRTC IRQ
	lea     nop_handler, %a0
	move.l  %a0, 0x138
	# CRTC H-SYNC
	lea     nop_handler, %a0
	move.l  %a0, 0x13c

	# TACR - event count mode
	move.b  #0x08, 0xe88019
	# TBCR - prescaler /4, don't change the setting, used by keyboard
	/* move.b  #0x01, 0xe8801b */
	# TCDCR - prescaler /200, /50
	move.b	#0x74, 0xe8801d

	# TADR
	move.b	#1, 0xe8801f
	# TBDR - don't change the setting, used by keyboard
	/* move.b	#100, 0xe88021 */
	# TCDR
	move.b	#200, 0xe88023
	# TDDR
	move.b	#80, 0xe88025

	# Timer A - event mode per V-DISP
	# Timer B - used by keyboard
	# Timer C - 4MHz / 200 / 200 = 100Hz, every 10ms
	# Timer D - 4MHz /  50 /  80 = 1000Hz, every 1ms

	# Enable interrupts - level 2
	move.w	#0x2000, %sr

	movem.l (%sp)+, %d2-%d7/%a2-%a6
	rts

save_interrupts:
	movem.l %d2-%d7/%a2-%a6, -(%sp)

	# Timer A
	lea		ss_save_data_base, %a0
	move.l	0x134, %d0
	move.l	%d0, (%a0)

	# Timer B
	move.l	0x120, %d0
	move.l	%d0, 4(%a0)

	# Timer C
	move.l	0x114, %d0
	move.l	%d0, 8(%a0)

	# Timer D
	move.l	0x110, %d0
	move.l	%d0, 12(%a0)

	# Key
	move.l	0x130, %d0
	move.l	%d0, 16(%a0)

	# save IERAB, IMRAB
	move.l	#0xe88007, %a1
	move.b	(%a1), %d0
	move.b	%d0, 20(%a0)

	move.l	#0xe88009, %a1
	move.b	(%a1), %d0
	move.b	%d0, 21(%a0)

	move.l	#0xe88013, %a1
	move.b	(%a1), %d0
	move.b	%d0, 22(%a0)

	move.l	#0xe88015, %a1
	move.b	(%a1), %d0
	move.b	%d0, 23(%a0)

	# TACR
	move.l	#0xe88019, %a1
	move.b	(%a1), %d0
	move.b	%d0, 24(%a0)
	# TBCR
	move.l	#0xe8801b, %a1
	move.b	(%a1), %d0
	move.b	%d0, 25(%a0)
	# TCDCR
	move.l	#0xe8801d, %a1
	move.b	(%a1), %d0
	move.b	%d0, 26(%a0)
	# TADR
	move.l	#0xe8801f, %a1
	move.b	(%a1), %d0
	move.b	%d0, 27(%a0)
	# TBDR
	move.l	#0xe88021, %a1
	move.b	(%a1), %d0
	move.b	%d0, 28(%a0)
	# TCDR
	move.l	#0xe88023, %a1
	move.b	(%a1), %d0
	move.b	%d0, 29(%a0)
	# TDDR
	move.l	#0xe88025, %a1
	move.b	(%a1), %d0
	move.b	%d0, 30(%a0)

	# CRTC IRQ
	move.l	0x138, %d0
	move.l	%d0, 34(%a0)
	# CRTC HSYNC
	move.l	0x13c, %d0
	move.l	%d0, 38(%a0)

	# reset interrupt masks
	move.w	#0x2700, %sr

	movem.l (%sp)+, %d2-%d7/%a2-%a6
	rts

restore_interrupts:
	movem.l %d2-%d7/%a2-%a6, -(%sp)

	# reset IPRAB, ISRAB
	move.b	#0x00, 0xe8800b
	move.b	#0x00, 0xe8800d
	move.b	#0x00, 0xe8800f
	move.b	#0x00, 0xe88011

	# Timer A
	lea		ss_save_data_base, %a0
	move.l	(%a0), %d0
	move.l	%d0, 0x134

	# Timer B - don't touch
	/* move.l	4(%a0), %d0 */
	/* move.l	%d0, 0x120 */

	# Timer C
	move.l	8(%a0), %d0
	move.l	%d0, 0x114

	# Timer D
	move.l	12(%a0), %d0
	move.l	%d0, 0x110

	# Key
	move.l	16(%a0), %d0
	move.l	%d0, 0x130

	# restore IERAB, IMRAB
	move.b	20(%a0), %d0
	move.b	%d0, 0xe88007

	move.b	21(%a0), %d0
	move.b	%d0, 0xe88009

	move.b	22(%a0), %d0
	move.b	%d0, 0xe88013

	move.b	23(%a0), %d0
	move.b	%d0, 0xe88015

	# TACR
	move.b	24(%a0), %d0
	move.b	%d0, 0xe88019
	# TBCR - don't touch
	/* move.b	25(%a0), %d0 */
	/* move.b	%d0, 0xe8801b */
	# TCDCR
	move.b	26(%a0), %d0
	move.b	%d0, 0xe8801d
	# TADR
	move.b	27(%a0), %d0
	move.b	%d0, 0xe8801f
	# TBDR - don't touch
	/* move.b	28(%a0), %d0 */
	/* move.b	%d0, 0xe88021 */
	# TCDR
	move.b	29(%a0), %d0
	move.b	%d0, 0xe88023
	# TDDR
	move.b	30(%a0), %d0
	move.b	%d0, 0xe88025

	# CRTC IRQ
	move.l	34(%a0), %d0
	move.l	%d0, 0x138
	# CRTC HSYNC
	move.l	38(%a0), %d0
	move.l	%d0, 0x13c

	movem.l (%sp)+, %d2-%d7/%a2-%a6
	rts

nop_handler:
    rte

vdisp_handler:
	movem.l	%d0/%a0, -(%sp)

	move.l	ss_timera_counter, %d0
	add.l 	#1, %d0
	move.l	%d0, ss_timera_counter

	movem.l	(%sp)+, %d0/%a0
	rte


usart_handler:
	movem.l	%d0/%a0, -(%sp)

	move.l	ss_timerb_counter, %d0
	add.l 	#1, %d0
	move.l	%d0, ss_timerb_counter

	movem.l	(%sp)+, %d0/%a0
	rte

timerc_handler:
	movem.l	%d0/%a0, -(%sp)

	move.l	ss_timerc_counter, %d0
	add.l 	#1, %d0
	move.l	%d0, ss_timerc_counter

	# do the same as IOCS's 0xFF1D46-8A
	lea.l	0x09b4.w, %a0
	subq.w 	#1, (%a0)
	bne.s	timerc_handler_l1_sub
timerc_handler_l1:
	move.w	-2(%a0), (%a0)+
	movea.l	(%a0), %a0
	jsr		(%a0)
	lea.l	0x09bc.w, %a0
	subq.w 	#1, (%a0)
	bne.s	timerc_handler_l2_sub
timerc_handler_l2:
	move.w	-2(%a0), (%a0)+
	movea.l	(%a0), %a0
	jsr		(%a0)
	lea.l	0x09c4.w, %a0
	subq.w	#1, (%a0)
	bne.s	timerc_handler_l3_sub
timerc_handler_l3:
	move.w	-2(%a0), (%a0)+
	movea.l	(%a0), %a0
	jsr		(%a0)
	lea.l	0x09cc.w, %a0
	subq.w	#1, (%a0)
	bne.s	timerc_handler_l4_sub
timerc_handler_l4:
	move.w	-2(%a0), (%a0)+
	movea.l	(%a0), %a0
	jsr		(%a0)

	movem.l	(%sp)+, %d0/%a0
	rte

# do the same as IOCS's 0xFF1D58-8F
timerc_handler_l1_sub:
	lea.l	0x09bc.w, %a0
	subq.w 	#1, (%a0)
	bne.s timerc_handler_l2_sub
	move.w	-2(%a0), (%a0)+
	movea.l	(%a0), %a0
	jsr		(%a0)
timerc_handler_l2_sub:
	lea.l	0x09c4.w, %a0
	subq.w	#1, (%a0)
	bne.s timerc_handler_l3_sub
	move.w	-2(%a0), (%a0)+
	movea.l	(%a0), %a0
	jsr		(%a0)
timerc_handler_l3_sub:
	lea.l	0x09cc.w, %a0
	subq.w	#1, (%a0)
	bne.s timerc_handler_l4_sub
	move.w	-2(%a0), (%a0)+
	movea.l	(%a0), %a0
	jsr		(%a0)
timerc_handler_l4_sub:
	movem.l	(%sp)+, %d0/%a0
	rte


timerd_handler:
	movem.l	%d0/%a0, -(%sp)

	move.l	ss_timerd_counter, %d0
	add.l 	#1, %d0
	move.l	%d0, ss_timerd_counter

	movem.l	(%sp)+, %d0/%a0
	rte

key_input_handler:
	movem.l	%d0/%a0, -(%sp)

	move.l	ss_key_counter, %d0
	add.l 	#1, %d0
	move.l	%d0, ss_key_counter

	movem.l	(%sp)+, %d0/%a0
	rte

 	.section .bss
	.even
ss_timera_counter:
	ds.l	1
ss_timerb_counter:
	ds.l	1
ss_timerc_counter:
	ds.l	1
ss_timerd_counter:
	ds.l	1
ss_key_counter:
	ds.l	1
ss_save_data_base:
 	ds.b	1024

	.end interrupts
