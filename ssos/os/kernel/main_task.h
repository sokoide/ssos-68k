#ifndef SS_MAIN_TASK_H
#define SS_MAIN_TASK_H

#include "scheduler.h"

/*
 * Register the C entry point's current execution context as a scheduler task.
 *
 * This is a bootstrap-only operation. Call it after ss_sched_init(), while
 * the caller is the current C entry point, and before any scheduler path can
 * select this TCB for resume. Both hosts meet that condition: standalone
 * registers before ss_set_interrupts(), baremetal masks interrupts around
 * registration. The first yield or Timer D switch saves the live C stack
 * before this TCB can be resumed; registration is not a general-purpose
 * way to enqueue an arbitrary suspended context.
 *
 * The function deliberately does not call ss_disable_interrupts() or
 * ss_enable_interrupts(): those assembly helpers force SR values instead of
 * restoring the caller's SR. The caller owns interrupt exclusion.
 */
uint16_t ss_main_task_register(SSTask* tcb, uint8_t pri);

#endif /* SS_MAIN_TASK_H */
