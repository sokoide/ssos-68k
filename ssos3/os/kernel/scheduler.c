#include "kernel.h"
#include "scheduler.h"
#include <stdint.h>
#include <string.h>

S3Task tcb_table[S3_MAX_TASKS];
S3ReadyQueue ready_queue;
void* s3_curr_task;
void* s3_scheduled_task;
uint16_t s3_task_count;

void s3_sched_init(void) {
    memset(tcb_table, 0, sizeof(tcb_table));
    memset(&ready_queue, 0, sizeof(ready_queue));
    s3_curr_task = NULL;
    s3_scheduled_task = NULL;
    s3_task_count = 0;
}

void s3_sched_enqueue(S3Task* tcb) {
    uint8_t pri = tcb->pri;
    if (pri >= S3_MAX_PRI) return;

    tcb->next = NULL;
    if (ready_queue.tails[pri] == NULL) {
        ready_queue.heads[pri] = tcb;
        ready_queue.tails[pri] = tcb;
        tcb->prev = NULL;
    } else {
        ready_queue.tails[pri]->next = tcb;
        tcb->prev = ready_queue.tails[pri];
        ready_queue.tails[pri] = tcb;
    }
    ready_queue.pri_bitmap |= (1 << (15 - pri));
}

void s3_sched_dequeue(S3Task* tcb) {
    uint8_t pri = tcb->pri;
    if (pri >= S3_MAX_PRI) return;

    if (tcb->prev) {
        tcb->prev->next = tcb->next;
    } else {
        ready_queue.heads[pri] = tcb->next;
    }
    if (tcb->next) {
        tcb->next->prev = tcb->prev;
    } else {
        ready_queue.tails[pri] = tcb->prev;
    }
    tcb->prev = NULL;
    tcb->next = NULL;

    if (ready_queue.heads[pri] == NULL) {
        ready_queue.pri_bitmap &= ~(1 << (15 - pri));
    }
}

S3Task* s3_sched_pick(void) {
    if (ready_queue.pri_bitmap == 0) return NULL;

    uint16_t bits = ready_queue.pri_bitmap;
    int pri = 0;
    while (!(bits & 0x8000) && pri < 16) {
        bits <<= 1;
        pri++;
    }
    if (pri >= S3_MAX_PRI) return NULL;
    return ready_queue.heads[pri];
}

uint16_t s3_task_create(S3TaskInfo* info) {
    if (info == NULL || info->entry == NULL) return S3_ERR_PARAM;
    if (info->pri >= S3_MAX_PRI) return S3_ERR_PARAM;

    s3_disable_interrupts();

    uint16_t i;
    for (i = 0; i < S3_MAX_TASKS; i++) {
        if (tcb_table[i].state == S3_TS_NONE) break;
    }
    if (i >= S3_MAX_TASKS) {
        s3_enable_interrupts();
        return S3_ERR_LIMIT;
    }

    S3Task* tcb = &tcb_table[i];
    memset(tcb, 0, sizeof(S3Task));
    tcb->state = S3_TS_DORMANT;
    tcb->entry = info->entry;
    tcb->pri = info->pri;
    tcb->ctx_level = info->ctx_level;

    if (info->stack != NULL) {
        tcb->stack_base = info->stack;
        tcb->stack_size = info->stack_size;
    } else {
        extern uint8_t* s3_task_stack_base;
        if (s3_task_stack_base == NULL) {
            s3_enable_interrupts();
            return S3_ERR_STATE;
        }
        uintptr_t stack_end = (uintptr_t)(s3_task_stack_base + (i + 1) * S3_TASK_STACK);
        tcb->stack_base = (uint8_t*)((stack_end - 4) & ~(uintptr_t)3);
        tcb->stack_size = S3_TASK_STACK;
    }

    tcb->context = tcb->stack_base;
    s3_task_count++;

    s3_enable_interrupts();
    return i + 1;  /* 1-based ID */
}

uint16_t s3_task_start(uint16_t id) {
    if (id == 0 || id > S3_MAX_TASKS) return S3_ERR_ID;

    s3_disable_interrupts();
    S3Task* tcb = &tcb_table[id - 1];

    if (tcb->state != S3_TS_DORMANT) {
        s3_enable_interrupts();
        return S3_ERR_STATE;
    }

    tcb->state = S3_TS_READY;
    s3_sched_enqueue(tcb);

    if (s3_curr_task == NULL) {
        s3_curr_task = tcb;
    }

    s3_enable_interrupts();
    return S3_OK;
}

void s3_do_context_switch(void) {
    S3Task* curr = (S3Task*)s3_curr_task;
    if (curr == NULL) return;

    /* Context SP is saved by the interrupt handler assembly */
    /* This function only handles the scheduling decision */

    S3Task* next = s3_sched_pick();
    if (next == NULL) return;
    if (next == curr) return;

    /* Round-robin: move current to end of its priority queue */
    s3_sched_dequeue(curr);
    s3_sched_enqueue(curr);

    s3_scheduled_task = next;
    s3_curr_task = next;
}

uint16_t s3_task_sleep(uint32_t ms) {
    S3Task* curr = (S3Task*)s3_curr_task;
    if (curr == NULL) return S3_ERR_STATE;

    s3_disable_interrupts();
    curr->state = S3_TS_WAIT;
    curr->wait_until = s3_tick_counter + ms;
    s3_sched_dequeue(curr);
    s3_enable_interrupts();

    /* Task will be woken by timer tick when wait_until expires */
    return S3_OK;
}

void s3_task_yield(void) {
    S3Task* curr = (S3Task*)s3_curr_task;
    if (curr == NULL) return;

    s3_disable_interrupts();
    s3_sched_dequeue(curr);
    s3_sched_enqueue(curr);
    s3_enable_interrupts();
}

void s3_process_wakeups(void) {
    uint32_t now = s3_tick_counter;
    for (uint16_t i = 0; i < S3_MAX_TASKS; i++) {
        S3Task* tcb = &tcb_table[i];
        if (tcb->state == S3_TS_WAIT && tcb->wait_until <= now) {
            tcb->state = S3_TS_READY;
            tcb->wait_until = 0;
            s3_sched_enqueue(tcb);
        }
    }
}
