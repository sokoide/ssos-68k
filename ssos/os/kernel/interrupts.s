	.include "iocscall.mac"

	.section .text
	.align	2
	.global	set_interrupts, restore_interrupts
	.global	disable_interrupts, enable_interrupts
	.global ss_timera_counter, ss_timerb_counter, ss_timerc_counter, ss_timerd_counter, ss_key_counter, ss_context_switch_counter
	.global ss_save_data_base
	.type	set_interrupts, @function
	.type	save_interrupts, @function
	.type	restore_interrupts, @function

disable_interrupts:
	move.w  #0x2700, %sr
	rts

enable_interrupts:
	move.w  #0x2000, %sr
	rts

set_interrupts:
	movem.l d2-d7/a2-a6, -(sp)

| Disable interrupts - level 7
	move.w	#0x2700, %sr

	bsr		save_interrupts

| MFP
| set MFP vector base (0x40 is the default)
| disable auto EQI -> need to reset ISRAB manually
	move.b  #0x41, 0xe88017

| AER, DDR
	/* move.b	#0x06, 0xe88003 */
	/* move.b	#0x00, 0xe88005 */

| IERAB
	move.b	#0xff, 0xe88007
	move.b	#0x7f, 0xe88009

| set IMRAB masks
	move.b	#0xff, 0xe88013
	move.b	#0x7f, 0xe88015

| reset IPRAB, ISRAB
	move.b	#0x00, 0xe8800b
	move.b	#0x00, 0xe8800d
	move.b	#0x00, 0xe8800f
	move.b	#0x00, 0xe88011

| set interrupt handlers
| Alarm
	/* move.l  a0, 0x100 */
| EXPWON
	/* move.l  a0, 0x104 */
| POWER SW
	/* move.l  a0, 0x108 */
| FM
	/* move.l  a0, 0x10c */
| Timer D
	lea		timerd_handler, a0
	move.l  a0, 0x110

| Timer C
| Used by IOCS's mouse handler, too
	/* lea		timerc_handler, a0 */
	/* move.l  a0, 0x114 */

| V-DISP state
	/* move.l  a0, 0x118 */

| 11c: not used by X68000
	/* lea     nop_handler, a0 */
	/* move.l  a0, 0x11c */

| USART, Timer B, don't change, used by keyboard
	/* lea		usart_handler, a0 */
	/* move.l  a0, 0x120 */
| key send error
	/* lea     nop_handler, a0 */
	/* move.l  a0, 0x124 */
| key send
	/* lea     nop_handler, a0 */
	/* move.l  a0, 0x128 */
	| key receive error */
	/* lea     nop_handler, a0 */
	/* move.l  a0, 0x12c */
| key receive - if you use IOCS key*, don't override this
	/* lea		key_input_handler, a0 */
	/* move.l  a0, 0x130 */
| CRTC V-DISP, Timer A
	lea		vdisp_handler, a0
	move.l  a0, 0x134
| CRTC IRQ
	lea     nop_handler, a0
	move.l  a0, 0x138
| CRTC H-SYNC
	lea     nop_handler, a0
	move.l  a0, 0x13c

| TACR - event count mode
	move.b  #0x08, 0xe88019
| TBCR - prescaler /4, don't change the setting, used by keyboard
	/* move.b  #0x01, 0xe8801b */
| TCDCR - prescaler *, /50
| only set D timer
	move.b	0xe8801d, d0
	or.b	0xF4, d0
	move.b	d0, 0xe8801d
| TCDCR - prescaler /200, /50
	/* move.b	#0x74, 0xe8801d */

| TADR
	move.b	#1, 0xe8801f
| TBDR - don't change the setting, used by keyboard
	/* move.b	#100, 0xe88021 */
| TCDR
	/* move.b	#200, 0xe88023 */
| TDDR
	move.b	#80, 0xe88025

| Timer A - event mode per V-DISP
| Timer B - used for keyboard in Human68K
| Timer C - used for cursor/FDD in Human68K
| Timer D - 4MHz /  50 /  80 = 1000Hz, every 1ms

| Enable interrupts - level 2
	move.w	#0x2000, %sr

	movem.l (sp)+, d2-d7/a2-a6
	rts

save_interrupts:
	movem.l d2-d7/a2-a6, -(sp)

| Timer A
	lea		ss_save_data_base, a0
	move.l	0x134, d0
	move.l	d0, (a0)

| Timer B
	move.l	0x120, d0
	move.l	d0, 4(a0)

| Timer C
	move.l	0x114, d0
	move.l	d0, 8(a0)

| Timer D
	move.l	0x110, d0
	move.l	d0, 12(a0)

| Key
	move.l	0x130, d0
	move.l	d0, 16(a0)

| save IERAB, IMRAB
	move.l	#0xe88007, a1
	move.b	(a1), d0
	move.b	d0, 20(a0)

	move.l	#0xe88009, a1
	move.b	(a1), d0
	move.b	d0, 21(a0)

	move.l	#0xe88013, a1
	move.b	(a1), d0
	move.b	d0, 22(a0)

	move.l	#0xe88015, a1
	move.b	(a1), d0
	move.b	d0, 23(a0)

| TACR
	move.l	#0xe88019, a1
	move.b	(a1), d0
	move.b	d0, 24(a0)
| TBCR
	move.l	#0xe8801b, a1
	move.b	(a1), d0
	move.b	d0, 25(a0)
| TCDCR
	move.l	#0xe8801d, a1
	move.b	(a1), d0
	move.b	d0, 26(a0)
| TADR
	move.l	#0xe8801f, a1
	move.b	(a1), d0
	move.b	d0, 27(a0)
| TBDR
	move.l	#0xe88021, a1
	move.b	(a1), d0
	move.b	d0, 28(a0)
| TCDR
	move.l	#0xe88023, a1
	move.b	(a1), d0
	move.b	d0, 29(a0)
| TDDR
	move.l	#0xe88025, a1
	move.b	(a1), d0
	move.b	d0, 30(a0)
| MFP vector
	move.l	#0xe88017, a1
	move.b	(a1), d0
	move.b	d0, 31(a0)

| CRTC IRQ
	move.l	0x138, d0
	move.l	d0, 34(a0)
| CRTC HSYNC
	move.l	0x13c, d0
	move.l	d0, 38(a0)

| reset interrupt masks
	move.w	#0x2700, %sr

	movem.l (sp)+, d2-d7/a2-a6
	rts

restore_interrupts:
	movem.l d2-d7/a2-a6, -(sp)

| Disable interrupts - level 7
	move.w	#0x2700, %sr

| reset IPRAB, ISRAB
	move.b	#0x00, 0xe8800b
	move.b	#0x00, 0xe8800d
	move.b	#0x00, 0xe8800f
	move.b	#0x00, 0xe88011

| Timer A
	lea		ss_save_data_base, a0
	move.l	(a0), d0
	move.l	d0, 0x134

| Timer B - don't touch
	/* move.l	4(a0), d0 */
	/* move.l	d0, 0x120 */

| Timer C
	/* move.l	8(a0), d0 */
	/* move.l	d0, 0x114 */

| Timer D
	move.l	12(a0), d0
	move.l	d0, 0x110

| Key
	move.l	16(a0), d0
	move.l	d0, 0x130

| restore IERAB, IMRAB
	move.b	20(a0), d0
	move.b	d0, 0xe88007

	move.b	21(a0), d0
	move.b	d0, 0xe88009

	move.b	22(a0), d0
	move.b	d0, 0xe88013

	move.b	23(a0), d0
	move.b	d0, 0xe88015

| TACR
	move.b	24(a0), d0
	move.b	d0, 0xe88019
| TBCR - don't touch
	/* move.b	25(a0), d0 */
	/* move.b	d0, 0xe8801b */
| TCDCR
	move.b	26(a0), d0
	move.b	d0, 0xe8801d
| TADR
	move.b	27(a0), d0
	move.b	d0, 0xe8801f
| TBDR - don't touch
	/* move.b	28(a0), d0 */
	/* move.b	d0, 0xe88021 */
| TCDR
	/* move.b	29(a0), d0 */
	/* move.b	d0, 0xe88023 */
| TDDR
	move.b	30(a0), d0
	move.b	d0, 0xe88025

| MFP vector
	move.b	31(a0), d0
	move.b	d0, 0xe88017

| CRTC IRQ
	move.l	34(a0), d0
	move.l	d0, 0x138
| CRTC HSYNC
	move.l	38(a0), d0
	move.l	d0, 0x13c

| reset interrupt masks
	move.w	#0x2700, %sr

	movem.l (sp)+, d2-d7/a2-a6
	rts

nop_handler:
    rte

vdisp_handler:
	movem.l	d0/a0, -(sp)

| reset ISRA's Timer A bit
	move.l	#0xe8800f, a0
	move.b	(a0), d0
	and.b	0xdf, d0
	move.b	d0, 0xe8800f

	add.l	#1, ss_timera_counter

	movem.l	(sp)+, d0/a0
	rte


usart_handler:
	movem.l	d0/a0, -(sp)

| reset ISRA's Timer B bit
	move.l	#0xe8800f, a0
	move.b	(a0), d0
	and.b	0xfe, d0
	move.b	d0, 0xe8800f

	add.l	#1, ss_timerb_counter

	movem.l	(sp)+, d0/a0
	rte

timerc_handler:
	movem.l	d0/a0, -(sp)

| reset ISRB's Timer C bit
	move.l	#0xe88011, a0
	move.b	(a0), d0
	and.b	0x5f, d0
	move.b	d0, 0xe8800f

	add.l	#1, ss_timerc_counter

| do the same as IOCS's 0xFF1D46-8A
	lea.l	0x09b4.w, a0
	subq.w 	#1, (a0)
	bne.s	timerc_handler_l1_sub
timerc_handler_l1:
	move.w	-2(a0), (a0)+
	movea.l	(a0), a0
	jsr		(a0)
	lea.l	0x09bc.w, a0
	subq.w 	#1, (a0)
	bne.s	timerc_handler_l2_sub
timerc_handler_l2:
	move.w	-2(a0), (a0)+
	movea.l	(a0), a0
	jsr		(a0)
	lea.l	0x09c4.w, a0
	subq.w	#1, (a0)
	bne.s	timerc_handler_l3_sub
timerc_handler_l3:
	move.w	-2(a0), (a0)+
	movea.l	(a0), a0
	jsr		(a0)
	lea.l	0x09cc.w, a0
	subq.w	#1, (a0)
	bne.s	timerc_handler_l4_sub
timerc_handler_l4:
	move.w	-2(a0), (a0)+
	movea.l	(a0), a0
	jsr		(a0)

	movem.l	(sp)+, d0/a0
	rte

| do the same as IOCS's 0xFF1D58-8F
timerc_handler_l1_sub:
	lea.l	0x09bc.w, a0
	subq.w 	#1, (a0)
	bne.s timerc_handler_l2_sub
	move.w	-2(a0), (a0)+
	movea.l	(a0), a0
	jsr		(a0)
timerc_handler_l2_sub:
	lea.l	0x09c4.w, a0
	subq.w	#1, (a0)
	bne.s timerc_handler_l3_sub
	move.w	-2(a0), (a0)+
	movea.l	(a0), a0
	jsr		(a0)
timerc_handler_l3_sub:
	lea.l	0x09cc.w, a0
	subq.w	#1, (a0)
	bne.s timerc_handler_l4_sub
	move.w	-2(a0), (a0)+
	movea.l	(a0), a0
	jsr		(a0)
timerc_handler_l4_sub:
	movem.l	(sp)+, d0/a0
	rte


| ============================================================
| TCB structure offsets (must match kernel.h TaskControlBlock)
| ============================================================
TCB_CONTEXT    = 0
TCB_TASK_ADDR  = 16
TCB_STACK_ADDR = 22

| ============================================================
| timerd_handler - Timer D interrupt handler (1ms interval)
| Handles timer counting and context switching
| ============================================================
timerd_handler:
	| Save ALL registers for full context switch support
	| 15 registers: d0-d7 (8) + a0-a6 (7) = 15 * 4 = 60 bytes
	movem.l	d0-d7/a0-a6, -(sp)

	add.l	#1, ss_timerd_counter

	jsr	timer_interrupt_handler

	tst	d0
	beq	skip_context_switch

	| Context switch needed
	| Stack: sp+0 = d0-d7/a0-a6 (60), sp+60 = PC (4), sp+64 = SR (2)
	bsr	context_switch

	| If we return here, context switch was not performed (curr_task NULL, etc)
	| Fall through to normal return

skip_context_switch:
	| Reset ISRB's Timer D bit (bit 4)
	move.l	#0xe88011, a0
	move.b	(a0), d0
	and.b	#0xef, d0
	move.b	d0, 0xe88011

	| Restore ALL registers and return from interrupt
	movem.l	(sp)+, d0-d7/a0-a6
	rte

| ============================================================
| context_switch - Preemptive context switch implementation
| Saves current task context and switches to next scheduled task
|
| Stack on entry (from timerd_handler via bsr):
|   sp+0  = return address (bsr pushed, 4 bytes)
|   sp+4  = d0-d7/a0-a6 (60 bytes saved by movem.l)
|   sp+64 = PC (4 bytes, exception frame)
|   sp+68 = SR (2 bytes, exception frame)
|
| Context saved to TCB: SP pointing to saved registers + exception frame
| (i.e., sp+4 on entry, which is where d0-d7/a0-a6 starts)
| ============================================================

.extern curr_task
.extern scheduled_task
.extern ss_scheduler

context_switch:
	add.l	#1, ss_context_switch_counter

	| Check if curr_task exists (NULL check)
	move.l	curr_task, d0
	beq.w	cs_no_curr_task		| curr_task is NULL, skip context save

	| Calculate SP to save: skip return address, point to saved registers
	| This is the SP that will be restored when switching back to this task
	move.l	sp, a0
	addq.l	#4, a0			| skip bsr return address

	| Save context SP to curr_task->context
	move.l	curr_task, a1
	move.l	a0, TCB_CONTEXT(a1)	| curr_task->context = SP (to saved regs)

cs_call_scheduler:
	| Call scheduler to determine next task
	jsr	ss_scheduler

	| Check if scheduled_task is available
	move.l	scheduled_task, d0
	beq.w	cs_no_next_task

	| Check if scheduled_task == curr_task (no switch needed)
	cmp.l	curr_task, d0
	beq.w	cs_return_to_handler	| same task, no switch

	| Switch to scheduled_task
	move.l	d0, a1			| a1 = scheduled_task
	move.l	a1, curr_task		| update curr_task

	| Get the task's context (saved SP or initial stack addr)
	move.l	TCB_CONTEXT(a1), a0	| a0 = scheduled_task->context

	| Check if this is a new task (context == stack_addr means never run)
	move.l	TCB_STACK_ADDR(a1), d0	| d0 = tcb->stack_addr
	cmp.l	a0, d0			| context == stack_addr?
	beq.w	cs_start_new_task	| yes - start new task

	| --------------------------------------------------------
	| Existing task - restore saved context
	| a0 = saved SP pointing to: d0-d7/a0-a6 (60) + PC (4) + SR (2)
	| --------------------------------------------------------

	| Clean up old stack's return address
	addq.l	#4, sp			| pop bsr return address

	| Reset MFP Timer D bit BEFORE switching stacks
	move.l	#0xe88011, a1
	move.b	(a1), d0
	and.b	#0xef, d0
	move.b	d0, 0xe88011

	| Switch to the restored task's stack
	move.l	a0, sp			| sp = saved context SP

	| Restore all registers and return to the interrupted task
	movem.l	(sp)+, d0-d7/a0-a6	| restore 60 bytes of registers
	rte				| pop PC+SR and return

cs_no_curr_task:
	| curr_task is NULL - no context to save, but try to start a new task
	bra.w	cs_call_scheduler

	| --------------------------------------------------------
	| New task - set up initial stack frame and start
	| a1 = scheduled_task TCB
	| --------------------------------------------------------
cs_start_new_task:
	| Clean up old stack's return address
	addq.l	#4, sp			| pop bsr return address

	| Reset MFP Timer D bit before switching stacks
	move.l	#0xe88011, a0
	move.b	(a0), d0
	and.b	#0xef, d0
	move.b	d0, 0xe88011

	| Get task entry point and stack address
	move.l	TCB_TASK_ADDR(a1), a0	| a0 = tcb->task_addr (entry point)
	move.l	TCB_STACK_ADDR(a1), sp	| sp = tcb->stack_addr (top of stack)

	| Build exception frame for RTE
	| M68000 RTE pops: (sp)+ -> SR, then (sp)+ -> PC
	| So we push PC first (ends up at lower address), then SR
	move.l	a0, -(sp)		| Push PC (task entry point)
	move.w	#0x2000, -(sp)		| Push SR (supervisor, interrupts enabled)

	| Note: For a new task, registers will have garbage values
	| This is acceptable - the task function will initialize what it needs
	| If you want zero-initialized registers, push 60 bytes of zeros here
	| and do movem.l (sp)+, d0-d7/a0-a6 before rte

	rte				| Start the new task

cs_no_next_task:
	| No next task available - continue with current task
	| Just return to caller, which falls through to skip_context_switch
	rts

cs_return_to_handler:
	| curr_task is NULL - just return to timerd_handler
	rts


key_input_handler:
	movem.l	d0/a0, -(sp)

	add.l	#1, ss_key_counter

	movem.l	(sp)+, d0/a0
	rte


	.section .data
	.even
ss_timera_counter:
	dc.l	0
ss_timerb_counter:
	dc.l	0
ss_timerc_counter:
	dc.l	0
ss_timerd_counter:
	dc.l	0
ss_key_counter:
	dc.l	0
ss_context_switch_counter:
	dc.l	0

 	.section .bss
	.even
ss_save_data_base:
 	ds.b	1024

	.end interrupts
