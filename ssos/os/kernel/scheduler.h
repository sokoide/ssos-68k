#ifndef SS_SCHEDULER_H
#define SS_SCHEDULER_H

#include <stddef.h>
#include "task.h"
#include "scheduler_abi.h"

/* Scheduler implementation/CPU ports/bootstrap only. Applications use task.h. */
/* Task states */
#define SS_TS_NONE     0
#define SS_TS_DORMANT  1
#define SS_TS_READY    2
#define SS_TS_WAIT     3
#define SS_TS_TERMINATED 4
#define SS_TS_INITIALIZING 5

typedef struct SSTask SSTask;
struct SSTask {
    void*    context;      /* Saved stack pointer */
    SSTask*  prev;
    SSTask*  next;
    void*    stack_base;
    uint32_t stack_size;
    void*    (*entry)(void*);
    uint32_t wait_until;
    uint8_t  state;
    uint8_t  pri;
    uint8_t  ctx_level;    /* SS_CTX_MINIMAL/NORMAL/FULL */
    uint8_t  resume_type;  /* 0 = interrupted, 1 = yielded */
    SSTask*  sleep_next;
    uint8_t  ipc_waiting;
    void*    arg;
};

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(SSTask, context) == SS_TCB_CONTEXT_OFFSET_VALUE,
               "SSTask.context ABI changed");
_Static_assert(offsetof(SSTask, stack_base) == SS_TCB_STACK_BASE_OFFSET_VALUE,
               "SSTask.stack_base ABI changed");
_Static_assert(offsetof(SSTask, entry) == SS_TCB_ENTRY_OFFSET_VALUE,
               "SSTask.entry ABI changed");
_Static_assert(offsetof(SSTask, resume_type) == SS_TCB_RESUME_TYPE_OFFSET_VALUE,
               "SSTask.resume_type ABI changed");
_Static_assert(offsetof(SSTask, sleep_next) == SS_TCB_SLEEP_NEXT_OFFSET_VALUE,
               "SSTask.sleep_next ABI changed");
_Static_assert(offsetof(SSTask, arg) == SS_TCB_ARG_OFFSET_VALUE,
               "SSTask.arg ABI changed");
_Static_assert(sizeof(SSTask) == SS_TCB_SIZE_VALUE,
               "SSTask size ABI changed");
#endif

typedef struct {
    uint16_t pri_bitmap;           /* bit 15 = pri 0 (highest) */
    SSTask*  heads[SS_MAX_PRI];
    SSTask*  tails[SS_MAX_PRI];
} SSReadyQueue;

extern SSTask tcb_table[];
extern SSReadyQueue ready_queue;
extern SSTask* ss_curr_task;
extern SSTask* ss_scheduled_task;
extern uint8_t* ss_task_stack_base;

void    ss_sched_init(void);
/*
 * Queue primitives do not manage interrupt state.  Bootstrap callers may
 * use them before interrupts are enabled; live scheduler callers must use
 * the existing critical-section convention.
 */
void    ss_sched_enqueue(SSTask* tcb);
void    ss_sched_dequeue(SSTask* tcb);
SSTask* ss_sched_pick(void);

void ss_do_context_switch(void);
void ss_do_wakeups(void);
void ss_process_wakeups(void);
void ss_stack_canary_init(uint16_t id);

/* ss_do_wakeups mutates sleeping/ready queues. Call only with IRQs masked;
 * ss_process_wakeups is invoked by ss_do_context_switch under that mask. */

#endif /* SS_SCHEDULER_H */
