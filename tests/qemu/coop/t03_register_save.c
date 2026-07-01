/* t03_register_save.c - verify the context switch preserves callee registers.
 *
 * This is the core correctness check Native tests and t02_round_robin cannot
 * do: each task keeps a distinct pattern in d2-d7 across many yields. If the
 * context switch (movem.l save on yield, movem.l restore on resume) drops or
 * corrupts any register, the mismatch is printed and the test stops.
 *
 * t02 only proves tasks *alternate* (via putchar side effects); it can't tell
 * you whether register state survived the round trip. This test does.
 *
 * Cooperative path: yield uses the manual SR/PC frame and resumes via jmp.
 * See pre/t02_register_save.c for the trap/rte equivalent.
 */

#include "scheduler.h"   /* kernel.h */
#include "tty.h"
#include <stdint.h>

#define GOAL 5

static volatile int n1 = 0, n2 = 0, failed = 0;

#define CHECK(task, r2,r3,r4,r5,r6,r7, A,B,C,D,E,F) \
    do { \
        if (r2!=(A)||r3!=(B)||r4!=(C)||r5!=(D)||r6!=(E)||r7!=(F)) { \
            tty_puts("\nFAIL " task " reg corrupted\n"); \
            failed = 1; for(;;) ss_task_yield(); \
        } \
    } while (0)

static void* task1(void* arg) {
    (void)arg;
    register uint32_t r2 asm("d2") = 0x11111111;
    register uint32_t r3 asm("d3") = 0x22222222;
    register uint32_t r4 asm("d4") = 0x33333333;
    register uint32_t r5 asm("d5") = 0x44444444;
    register uint32_t r6 asm("d6") = 0x55555555;
    register uint32_t r7 asm("d7") = 0x66666666;

    for (;;) {
        CHECK("task1", r2,r3,r4,r5,r6,r7,
              0x11111111,0x22222222,0x33333333,0x44444444,0x55555555,0x66666666);
        n1++;
        tty_putc('1');
        ss_task_yield();
    }
    return 0;
}

static void* task2(void* arg) {
    (void)arg;
    register uint32_t r2 asm("d2") = 0xAAAAAAAA;
    register uint32_t r3 asm("d3") = 0xBBBBBBBB;
    register uint32_t r4 asm("d4") = 0xCCCCCCCC;
    register uint32_t r5 asm("d5") = 0xDDDDDDDD;
    register uint32_t r6 asm("d6") = 0xEEEEEEEE;
    register uint32_t r7 asm("d7") = 0x76543210;

    for (;;) {
        CHECK("task2", r2,r3,r4,r5,r6,r7,
              0xAAAAAAAA,0xBBBBBBBB,0xCCCCCCCC,0xDDDDDDDD,0xEEEEEEEE,0x76543210);
        n2++;
        tty_putc('2');
        ss_task_yield();
    }
    return 0;
}

static SSTask main_tcb;

int main(void) {
    tty_puts("START reg-save\n");

    ss_sched_init();
    main_tcb.pri        = 8;
    main_tcb.stack_base = (void*)1;
    main_tcb.state      = SS_TS_READY;
    ss_curr_task        = &main_tcb;
    ss_sched_enqueue(&main_tcb);

    SSTaskInfo t1 = { .entry = task1, .pri = 8, .stack_size = SS_TASK_STACK, .stack = NULL };
    SSTaskInfo t2 = { .entry = task2, .pri = 8, .stack_size = SS_TASK_STACK, .stack = NULL };
    uint16_t id1 = ss_task_create(&t1);
    uint16_t id2 = ss_task_create(&t2);
    ss_task_start(id1);
    ss_task_start(id2);

    while ((n1 < GOAL || n2 < GOAL) && !failed) {
        ss_task_yield();
    }

    if (!failed) {
        tty_puts("\nOK regs preserved\n");
    }
    for (;;) { }
    return 0;
}
