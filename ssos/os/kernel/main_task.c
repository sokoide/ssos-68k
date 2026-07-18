#include "main_task.h"

#include <string.h>

/*
 * The context switch assembly recognizes a created task only when
 * context == stack_base and entry is non-NULL. The C entry point has no
 * synthetic start frame or entry, so it uses the same sentinel until its
 * first yield or Timer D switch saves the live stack into context. The
 * bootstrap ordering still matters: registration is valid only while this
 * TCB is ss_curr_task; a switch saves ss_curr_task before scheduler selection.
 * This TCB is intentionally outside tcb_table: it is not allocated by
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
