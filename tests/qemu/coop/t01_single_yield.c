/* t01_single_yield.c - smallest cooperative scheduler test.
 *
 * One worker task. main starts it, the worker prints 'T' and yields (returning
 * to main), then main prints 'M'. Seeing both letters proves the very first
 * context switch works: main registers as a task, starts the worker, and a
 * single yield round-trips control between them via movem.l/jmp.
 *
 * This is the "hello world" of the cooperative scheduler — read this before
 * t02_round_robin.c (two tasks) and t03_register_save.c (register integrity).
 */

#include "scheduler.h"   /* kernel.h: SSTask, SSTaskInfo, API */
#include "tty.h"

static void* worker(void* arg) {
    (void)arg;
    tty_putc('T');
    ss_task_yield();        /* hand control back to main */
    /* We resume here when main next yields to us; just loop quietly. */
    for (;;) {
        ss_task_yield();
    }
    return 0;
}

static SSTask main_tcb;

int main(void) {
    tty_puts("START single-yield\n");

    ss_sched_init();

    main_tcb.pri        = 8;
    main_tcb.stack_base = (void*)1;   /* sentinel: defeats new-task detection */
    main_tcb.state      = SS_TS_READY;
    ss_curr_task        = &main_tcb;
    ss_sched_enqueue(&main_tcb);

    SSTaskInfo info = {
        .entry = worker, .pri = 8, .ctx_level = 0,
        .stack_size = SS_TASK_STACK, .stack = NULL,
    };
    uint16_t id = ss_task_create(&info);
    ss_task_start(id);

    /* main yields -> worker runs ('T') -> worker yields -> main resumes. */
    ss_task_yield();

    tty_putc('M');
    tty_puts("\nOK\n");

    for (;;) { }
    return 0;
}
