/* t02_round_robin.c - cooperative scheduler test driving the REAL SSOS
 * scheduler under QEMU.
 *
 * This is the test Native tests cannot do: the SSOS scheduler.c and the
 * ctx switch (ctx_switch.s, a port of interrupts.s) run on a real m68k, so
 * tasks genuinely swap register state via movem.l. Two tasks yield back and
 * forth in a round-robin with the main task; we prove the switches happen by
 * printing '1' / '2' as each task runs, then 'OK' once both have hit the goal.
 *
 * Boot strap (mirrors standalone/main.c): main registers itself as a task with
 * stack_base=1 so the new-task detection (context == stack_base) never fires
 * for it, sets ss_curr_task, then creates+starts the two workers.
 */

#include "scheduler.h"   /* pulls in kernel.h: SSTask, SSTaskInfo, API */
#include "tty.h"
#include <stdint.h>

#define GOAL 6

static volatile unsigned task1_n = 0;
static volatile unsigned task2_n = 0;

static void* task1_entry(void* arg) {
    (void)arg;
    for (;;) {
        task1_n++;
        tty_putc('1');
        ss_task_yield();      /* -> task2 (and main) round-robin */
    }
    return 0;
}

static void* task2_entry(void* arg) {
    (void)arg;
    for (;;) {
        task2_n++;
        tty_putc('2');
        ss_task_yield();      /* -> main / task1 round-robin */
    }
    return 0;
}

/* main's own TCB. stack_base=1 defeats .start_task's new-task check. */
static SSTask main_tcb;

int main(void) {
    tty_puts("START\n");

    ss_sched_init();

    main_tcb.pri        = 8;
    main_tcb.stack_base = (void*)1;   /* not a real stack; just a sentinel */
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

    /* Yield rounds with the workers until both reach GOAL. Interleaving is
     * strict round-robin, so task1_n and task2_n stay within 1 of each other. */
    while (task1_n < GOAL || task2_n < GOAL) {
        ss_task_yield();
    }

    tty_puts("\nOK task1=");
    tty_putu(task1_n);
    tty_puts(" task2=");
    tty_putu(task2_n);
    tty_puts("\n");

    /* Park; the QEMU timeout reaps the process. */
    for (;;) { }
    return 0;
}
