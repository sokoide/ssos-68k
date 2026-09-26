#ifndef SS_SCHEDULER_H
#define SS_SCHEDULER_H

#include "kernel.h"

#define SS_MAX_SLEEP_TICKS 0x7FFFFFFFUL

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
_Static_assert(offsetof(SSTask, context) == 0, "SSTask.context ABI changed");
_Static_assert(offsetof(SSTask, stack_base) == 12, "SSTask.stack_base ABI changed");
_Static_assert(offsetof(SSTask, entry) == 20, "SSTask.entry ABI changed");
_Static_assert(offsetof(SSTask, resume_type) == 31, "SSTask.resume_type ABI changed");
_Static_assert(offsetof(SSTask, sleep_next) == 32, "SSTask.sleep_next ABI changed");
_Static_assert(offsetof(SSTask, arg) == 38, "SSTask.arg ABI changed");
#endif

typedef struct {
    void* (*entry)(void*); /* entry(arg); return value is ignored */
    uint8_t pri;
    uint8_t ctx_level;
    uint16_t stack_size;   /* custom stack must be >= SS_MIN_TASK_STACK */
    void*    stack;        /* NULL = pool; otherwise last aligned word of region */
    void*    arg;          /* passed to entry; NULL by default */
} SSTaskInfo;

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

uint16_t ss_task_create(SSTaskInfo* info);
uint16_t ss_task_start(uint16_t id);
void     ss_do_context_switch(void);
void     ss_do_wakeups(void);
uint16_t ss_task_sleep(uint32_t ticks);
void     ss_task_yield(void);
void     ss_process_wakeups(void);
void     ss_task_exit(void) __attribute__((noreturn));

/* Custom stack is a descending region [stack - stack_size + 4, stack + 4).
 * It remains caller-owned until system shutdown; it is not reclaimed.
 * Returning from entry calls ss_task_exit. The task becomes TERMINATED and
 * its slot/stack remain reserved; there is no join or stack reclamation yet.
 * ss_task_create returns IDs 1..SS_MAX_TASKS or a uint16_t-encoded SS_ERR_*.
 * ctx_level is accepted but all registers are currently saved at every switch.
 */

/* ss_do_wakeups mutates sleeping/ready queues. Call only with IRQs masked;
 * ss_process_wakeups is invoked by ss_do_context_switch under that mask. */

/* Stack debugging */
uint32_t ss_stack_check(uint16_t id);
void     ss_stack_canary_init(uint16_t id);

#endif /* SS_SCHEDULER_H */
