| ============================================================
| SSOS2 - Timer D ISR + Mouse Reader + Context Switch
|
| timerd_isr:   1ms tick counter
| get_mouse_delta: read mouse via IOCS _MS_GETDT
| task_switch_to: cooperative context switch
| ============================================================

    .section .text

| ============================================================
| Timer D Interrupt Handler
|
| Timer D fires at 1000Hz (1ms interval).
| Increments ssos2_tick for the GUI counter display.
|
| Timer D ISR vector is at address 0x110
| (MFP vector base 0x40 + Timer D offset 4 = 0x44, × 4 = 0x110)
| ============================================================

    .global timerd_isr
    .extern ssos2_tick

timerd_isr:
    movem.l d0/a0, -(sp)

    add.l   #1, ssos2_tick

    | Acknowledge Timer D: clear ISRB bit 4
    move.l  #0xe88011, a0
    move.b  (a0), d0
    and.b   #0xef, d0
    move.b  d0, (a0)

    movem.l (sp)+, d0/a0
    rte

| ============================================================
| Mouse state reader (absolute position + buttons)
| Calls IOCS _MS_STAT (0x7D).
|
| void get_mouse_stat(int16_t *x, int16_t *y, int16_t *buttons)
| Stack (after movem of 5 regs = 20 bytes):
|   sp+20: return address
|   sp+24: x pointer
|   sp+28: y pointer
|   sp+32: buttons pointer
|
| _MS_STAT returns:
|   D0.L = button state (bit0=right, bit1=left, 0=pressed)
|   D1.L = X coordinate (pixels)
|   D2.L = Y coordinate (pixels)
| ============================================================

    .global get_mouse_stat

get_mouse_stat:
    movem.l d1-d2/a0-a2, -(sp)

    move.l  24(sp), a0       | x pointer
    move.l  28(sp), a1       | y pointer
    move.l  32(sp), a2       | buttons pointer

    | _MS_STAT (0x7D): D0=buttons, D1=x, D2=y
    moveq   #0x7d, d0
    trap    #15

    move.w  d1, (a0)         | store x
    move.w  d2, (a1)         | store y
    move.w  d0, (a2)         | store buttons

    movem.l (sp)+, d1-d2/a0-a2
    rts

| ============================================================
| Cooperative context switch
|
| void task_switch_to(uint8_t** current_sp_ptr, uint8_t* next_sp)
|
| Saves callee-saved registers (d2-d7, a2-a6) on the current stack,
| stores SP into *current_sp_ptr, then loads next_sp and restores
| registers from it.  The rts at the end returns into the new task.
|
| Stack (after movem of 11 regs = 44 bytes):
|   sp+0..sp+43 : saved registers
|   sp+44       : return address
|   sp+48       : arg0 = current_sp_ptr
|   sp+52       : arg1 = next_sp
| ============================================================

    .global task_switch_to

task_switch_to:
    movem.l d2-d7/a2-a6, -(sp)    | save 11 callee-saved regs (44 bytes)

    move.l  48(sp), a0             | a0 = current_sp_ptr
    move.l  sp, (a0)               | *current_sp_ptr = saved SP
    move.l  52(sp), sp             | switch to new task's stack

    movem.l (sp)+, d2-d7/a2-a6    | restore regs from new stack
    rts                            | "return" into new task
