	.include "iocscall.mac"

	.section .text
	.align	2
	.globl	boot
	.type	boot, @function

boot:
	bra.w	entry

entry:
	lea		mysp, a7

	move.w	#0, d1
	move.w	#0, d2

	IOCS	_B_LOCATE
	IOCS	_OS_CURON

	lea.l	.message1, a1
	IOCS	_B_PRINT

	lea.l	.message2, a1
	IOCS	_B_PRINT

	# read from the sector 2 to 8
	#  track 0, side 0, sector 1: IPL
	#  track 0, side 0, sector 2: <- read from here
	move.l	#0x010000, a1
	# 0x90: 2HD 0
	# 0x70: MFM, retry, seek
	move.w	#0x9070, d1
	# 03: 2HD
	# 00: Track 0
	# 00: Side 0
	# 02: Sector 2
	move.l	#0x03000002, d2
	/* # read 7 sectors */
	/* move.l	#0x1c00, d3 */
	# read 7 (track 0, side 0) + 8 (track 0, side 1) sectors
	# + 8 sectors * 76 tracks * 2 sides
	#move.l	#1024*(7+8+8*76*2), d3
	#
	# 7: 7 sectors in track 0 side 0
	# 8: 8 sectors in track 0 side 1
	# 8*12*2: 8 sectors in track 1-13 x 2 sides
	# Total 8*(1+1+24)-1 = 207KB
	move.l	#1024*(7+8+8*12*2), d3

	IOCS	_B_READ
	# Mask FDC result status 0's low bits & cylinder
	and.l	#0xF0FFFF00, d0
	tst.l	d0
	bne.w 	err

done:
	lea.l	.message_done, a1
	IOCS	_B_PRINT
	move.l	#0x010000, a2
	jmp		(a2)

err:
	move.l	#0x010000, a2
	move.l	d0, (a2)
	lea.l	.message_err, a1
	IOCS	_B_PRINT

loop:
	jmp		loop

	.section .data
.message1:
	.string	"Booting Scott & Sandy OS...\r\n"
	.even

.message2:
	.string	"Loading the disk...\r\n"
	.even

.message_done:
	.string	"Sector 2 copied to 0x010000. Ready to go!\r\n"
	.even

.message_err:
	.string	"Error\r\n"
	.even

	.section .bss

mystack:
	ds.b	256
mysp:
	.end boot
