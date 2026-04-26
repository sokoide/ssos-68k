#ifndef S4_SCHEDULER_H
#define S4_SCHEDULER_H

#include "kernel.h"

typedef struct S4Task S4Task;
struct S4Task {
    void*    context;      /* Saved stack pointer */
    S4Task*  prev;
    S4Task*  next;
    void*    stack_base;
    uint32_t stack_size;
    void*    (*entry)(void*);
    uint32_t wait_until;
    uint8_t  state;
    uint8_t  pri;
    uint8_t  ctx_level;    /* S4_CTX_MINIMAL/NORMAL/FULL */
    uint8_t  pad;
};

typedef struct {
    void* (*entry)(void*);
    uint8_t pri;
    uint8_t ctx_level;
    uint16_t stack_size;
    void*    stack;        /* NULL = auto-allocate */
} S4TaskInfo;

typedef struct {
    uint16_t pri_bitmap;           /* bit 15 = pri 0 (highest) */
    S4Task*  heads[S4_MAX_PRI];
    S4Task*  tails[S4_MAX_PRI];
} S4ReadyQueue;

extern S4Task tcb_table[];
extern S4ReadyQueue ready_queue;
extern void* s4_curr_task;
extern void* s4_scheduled_task;
extern uint8_t* s4_task_stack_base;

void    s4_sched_init(void);
void    s4_sched_enqueue(S4Task* tcb);
void    s4_sched_dequeue(S4Task* tcb);
S4Task* s4_sched_pick(void);

uint16_t s4_task_create(S4TaskInfo* info);
uint16_t s4_task_start(uint16_t id);
void     s4_do_context_switch(void);
uint16_t s4_task_sleep(uint32_t ms);
void     s4_task_yield(void);
void     s4_process_wakeups(void);

#endif /* S4_SCHEDULER_H */
