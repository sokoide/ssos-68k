#ifndef SS_SCHEDULER_H
#define SS_SCHEDULER_H

#include "kernel.h"

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
    uint8_t  pad;
};

typedef struct {
    void* (*entry)(void*);
    uint8_t pri;
    uint8_t ctx_level;
    uint16_t stack_size;
    void*    stack;        /* NULL = auto-allocate */
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

/* Stack debugging */
uint32_t ss_stack_check(uint16_t id);
void     ss_stack_canary_init(uint16_t id);

#endif /* SS_SCHEDULER_H */
