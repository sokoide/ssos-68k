/* t01_round_robin.c - preemptive (Timer D ISR) scheduler test under QEMU.
 *
 * This is the test Phase B and Native tests cannot do: drive the SSOS
 * preemptive scheduler through the REAL ISR path. The unmodified
 * preemptive/scheduler.c runs on a real m68k; preempt_ctx_switch.s is the
 * Timer D handler (driven via trap #0) plus the resume_task/rte machinery.
 *
 * main + two workers all fire `trap #0` to trigger preemption. Each trap
 * enters the ISR, which calls ss_do_wakeups + ss_do_context_switch and returns
 * via `.resume_interrupted` -> `rte`, landing in whichever task the scheduler
 * picked next. Seeing `1212...` proves tasks swap via the rte path (a CPU
 * exception frame being popped), not just a cooperative jmp.
 *
 * Scope: trap is a *synchronous* exception (the task fires it itself), so this
 * is not a true asynchronous hardware preemption. It validates the ISR-driven
 * context-switch mechanics, which is the part Native cannot reach.
 */

#include "scheduler.h"   /* kernel.h: SSTask, SSTaskInfo, API */
#include "tty.h"
#include <stdint.h>

#define GOAL 6

static volatile unsigned task1_n = 0;
static volatile unsigned task2_n = 0;

/* Fire the Timer D ISR simulation. trap #0 -> vector 0x80 -> ss_timerd_handler
 * (set up in preempt_ctx_switch.s:_start). The exception frame it pushes is
 * what `rte` later pops to land in the next task. */
static inline void emulate_timer_tick(void) {
    __asm__ volatile ("trap #0" ::: "memory");
}

static void* task1_entry(void* arg) {
    (void)arg;
    for (;;) {
        task1_n++;
        tty_putc('1');
        emulate_timer_tick();     /* preemption point -> task2 / main */
    }
    return 0;
}

static void* task2_entry(void* arg) {
    (void)arg;
    for (;;) {
        task2_n++;
        tty_putc('2');
        emulate_timer_tick();     /* preemption point -> main / task1 */
    }
    return 0;
}

static SSTask main_tcb;

int main(void) {
    tty_puts("START preempt\n");

    ss_sched_init();

    main_tcb.pri        = 8;
    main_tcb.stack_base = (void*)1;   /* sentinel: not a real stack */
    main_tcb.state      = SS_TS_READY;
    ss_curr_task        = &main_tcb;
    ss_sched_enqueue(&main_tcb);

    SSTaskInfo t1 = {
        .entry = task1_entry, .pri = 8, .ctx_level = 0,
        .stack_size = SS_TASK_STACK, .stack = NULL,
    };
    SSTaskInfo t2 = {
        .entry = task2_entry, .pri = 8, .ctx_level = 0,
        .stack_size = SS_TASK_STACK, .stack = NULL,
    };
    uint16_t id1 = ss_task_create(&t1);
    uint16_t id2 = ss_task_create(&t2);
    ss_task_start(id1);
    ss_task_start(id2);

    /* main joins the round-robin by firing trap #0 itself. Workers and main
     * take turns; workers print 1/2, main watches the counters. */
    while (task1_n < GOAL || task2_n < GOAL) {
        emulate_timer_tick();
    }

    tty_puts("\nOK task1=");
    tty_putu(task1_n);
    tty_puts(" task2=");
    tty_putu(task2_n);
    tty_puts("\n");

    for (;;) { }
    return 0;
}
