#include "kernel.h"
#include "scheduler.h"
#include <stdint.h>
#include <string.h>

S4Task tcb_table[S4_MAX_TASKS];
S4ReadyQueue ready_queue;
void* s4_curr_task;
void* s4_scheduled_task;
uint16_t s4_task_count;

void s4_sched_init(void) {
    memset(tcb_table, 0, sizeof(tcb_table));
    memset(&ready_queue, 0, sizeof(ready_queue));
    s4_curr_task = NULL;
    s4_scheduled_task = NULL;
    s4_task_count = 0;
}

void s4_sched_enqueue(S4Task* tcb) {
    uint8_t pri = tcb->pri;
    if (pri >= S4_MAX_PRI) return;

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

void s4_sched_dequeue(S4Task* tcb) {
    uint8_t pri = tcb->pri;
    if (pri >= S4_MAX_PRI) return;

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

S4Task* s4_sched_pick(void) {
    if (ready_queue.pri_bitmap == 0) return NULL;

    uint16_t bits = ready_queue.pri_bitmap;
    int pri = 0;
    while (!(bits & 0x8000) && pri < 16) {
        bits <<= 1;
        pri++;
    }
    if (pri >= S4_MAX_PRI) return NULL;
    return ready_queue.heads[pri];
}

uint16_t s4_task_create(S4TaskInfo* info) {
    if (info == NULL || info->entry == NULL) return S4_ERR_PARAM;
    if (info->pri >= S4_MAX_PRI) return S4_ERR_PARAM;

    s4_disable_interrupts();

    uint16_t i;
    for (i = 0; i < S4_MAX_TASKS; i++) {
        if (tcb_table[i].state == S4_TS_NONE) break;
    }
    if (i >= S4_MAX_TASKS) {
        s4_enable_interrupts();
        return S4_ERR_LIMIT;
    }

    S4Task* tcb = &tcb_table[i];
    memset(tcb, 0, sizeof(S4Task));
    tcb->state = S4_TS_DORMANT;
    tcb->entry = info->entry;
    tcb->pri = info->pri;
    tcb->ctx_level = info->ctx_level;

    if (info->stack != NULL) {
        tcb->stack_base = info->stack;
        tcb->stack_size = info->stack_size;
    } else {
        extern uint8_t* s4_task_stack_base;
        if (s4_task_stack_base == NULL) {
            s4_enable_interrupts();
            return S4_ERR_STATE;
        }
        uintptr_t stack_end = (uintptr_t)(s4_task_stack_base + (i + 1) * S4_TASK_STACK);
        tcb->stack_base = (uint8_t*)((stack_end - 4) & ~(uintptr_t)3);
        tcb->stack_size = S4_TASK_STACK;
    }

    tcb->context = tcb->stack_base;
    s4_task_count++;

    s4_enable_interrupts();
    return i + 1;  /* 1-based ID */
}

uint16_t s4_task_start(uint16_t id) {
    if (id == 0 || id > S4_MAX_TASKS) return S4_ERR_ID;

    s4_disable_interrupts();
    S4Task* tcb = &tcb_table[id - 1];

    if (tcb->state != S4_TS_DORMANT) {
        s4_enable_interrupts();
        return S4_ERR_STATE;
    }

    tcb->state = S4_TS_READY;
    s4_sched_enqueue(tcb);

    if (s4_curr_task == NULL) {
        s4_curr_task = tcb;
    }

    s4_enable_interrupts();
    return S4_OK;
}

void s4_do_context_switch(void) {
    S4Task* curr = (S4Task*)s4_curr_task;
    if (curr == NULL) return;

    /* Context SP is saved by the interrupt handler assembly */
    /* This function only handles the scheduling decision */

    S4Task* next = s4_sched_pick();
    if (next == NULL || next == curr) {
        s4_scheduled_task = curr;
        return;
    }

    /* Round-robin: move current to end of its priority queue */
    s4_sched_dequeue(curr);
    s4_sched_enqueue(curr);

    s4_scheduled_task = next;
    s4_curr_task = next;
}

uint16_t s4_task_sleep(uint32_t ms) {
    S4Task* curr = (S4Task*)s4_curr_task;
    if (curr == NULL) return S4_ERR_STATE;

    s4_disable_interrupts();
    curr->state = S4_TS_WAIT;
    curr->wait_until = s4_tick_counter + ms;
    s4_sched_dequeue(curr);
    s4_enable_interrupts();

    /* Task will be woken by timer tick when wait_until expires */
    return S4_OK;
}

void s4_task_yield(void) {
    S4Task* curr = (S4Task*)s4_curr_task;
    if (curr == NULL) return;

    s4_disable_interrupts();
    s4_sched_dequeue(curr);
    s4_sched_enqueue(curr);
    s4_enable_interrupts();
}

void s4_process_wakeups(void) {
    uint32_t now = s4_tick_counter;
    for (uint16_t i = 0; i < S4_MAX_TASKS; i++) {
        S4Task* tcb = &tcb_table[i];
        if (tcb->state == S4_TS_WAIT && tcb->wait_until <= now) {
            tcb->state = S4_TS_READY;
            tcb->wait_until = 0;
            s4_sched_enqueue(tcb);
        }
    }
}
