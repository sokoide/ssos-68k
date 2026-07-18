#include "main_task.h"

#include <string.h>

/*
 * The context switch assembly recognizes context == stack_base as a task
 * that has never run and jumps to its entry point.  The current C entry point
 * already has a live stack, so a non-stack sentinel keeps it on the resume
 * path.  This TCB is intentionally outside tcb_table: it is not allocated by
 * ss_task_create() and must not consume a worker-task slot.
 */
#define SS_MAIN_TASK_STACK_SENTINEL ((void*)1)

uint16_t ss_main_task_register(SSTask* tcb, uint8_t pri) {
    if (tcb == NULL || pri >= SS_MAX_PRI) return SS_ERR_PARAM;
    if (ss_curr_task != NULL) return SS_ERR_STATE;

    memset(tcb, 0, sizeof(*tcb));
    tcb->context = SS_MAIN_TASK_STACK_SENTINEL;
    tcb->stack_base = SS_MAIN_TASK_STACK_SENTINEL;
    tcb->state = SS_TS_READY;
    tcb->pri = pri;

    ss_curr_task = tcb;
    ss_sched_enqueue(tcb);
    return SS_OK;
}
