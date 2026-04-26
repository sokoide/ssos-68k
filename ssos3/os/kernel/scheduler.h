#ifndef S3_SCHEDULER_H
#define S3_SCHEDULER_H

#include "kernel.h"

typedef struct S3Task S3Task;
struct S3Task {
    void*    context;      /* Saved stack pointer */
    S3Task*  prev;
    S3Task*  next;
    void*    stack_base;
    uint32_t stack_size;
    void*    (*entry)(void*);
    uint32_t wait_until;
    uint8_t  state;
    uint8_t  pri;
    uint8_t  ctx_level;    /* S3_CTX_MINIMAL/NORMAL/FULL */
    uint8_t  pad;
};

typedef struct {
    void* (*entry)(void*);
    uint8_t pri;
    uint8_t ctx_level;
    uint16_t stack_size;
    void*    stack;        /* NULL = auto-allocate */
} S3TaskInfo;

typedef struct {
    uint16_t pri_bitmap;           /* bit 15 = pri 0 (highest) */
    S3Task*  heads[S3_MAX_PRI];
    S3Task*  tails[S3_MAX_PRI];
} S3ReadyQueue;

extern S3Task tcb_table[];
extern S3ReadyQueue ready_queue;
extern void* s3_curr_task;
extern void* s3_scheduled_task;
extern uint8_t* s3_task_stack_base;

void    s3_sched_init(void);
void    s3_sched_enqueue(S3Task* tcb);
void    s3_sched_dequeue(S3Task* tcb);
S3Task* s3_sched_pick(void);

uint16_t s3_task_create(S3TaskInfo* info);
uint16_t s3_task_start(uint16_t id);
void     s3_do_context_switch(void);
uint16_t s3_task_sleep(uint32_t ms);
void     s3_task_yield(void);
void     s3_process_wakeups(void);

#endif /* S3_SCHEDULER_H */
