#ifndef SS_MAIN_TASK_H
#define SS_MAIN_TASK_H

#include "scheduler.h"

/*
 * Register the C entry point's current execution context as a scheduler task.
 *
 * This is a bootstrap-only operation.  Call it after ss_sched_init() and
 * before the scheduler/interrupts can run.  The function deliberately does
 * not call ss_disable_interrupts()/ss_enable_interrupts(): those assembly
 * helpers force SR values instead of restoring the caller's SR.
 */
uint16_t ss_main_task_register(SSTask* tcb, uint8_t pri);

#endif /* SS_MAIN_TASK_H */
