#include "kernel.h"
#include "scheduler.h"
#include <stdint.h>
#include <string.h>

SSTask tcb_table[SS_MAX_TASKS];
SSReadyQueue ready_queue;
SSTask* ss_curr_task;
SSTask* ss_scheduled_task;
uint16_t ss_task_count;

/* Stack canary magic number */
#define STACK_CANARY 0xDEADCAFE

/* Write canary at bottom of each task stack, return remaining bytes */
uint32_t ss_stack_check(uint16_t id) {
    if (id == 0 || id > SS_MAX_TASKS) return 0;
    SSTask* tcb = &tcb_table[id - 1];
    if (tcb->stack_size == 0) return 0;

    /* Bottom of stack = stack_base - stack_size + 4 */
    uint8_t* bottom = (uint8_t*)((uintptr_t)tcb->stack_base - tcb->stack_size + 4);
    uint32_t* canary = (uint32_t*)bottom;

    /* Scan from bottom upward to find first non-canary word */
    uint32_t used = 0;
    for (uint32_t offset = 0; offset < tcb->stack_size; offset += 4) {
        if (*(uint32_t*)(bottom + offset) != STACK_CANARY) {
            used = tcb->stack_size - offset;
            break;
        }
    }
    return tcb->stack_size - used;  /* remaining bytes */
}

/* Write canary pattern to entire task stack */
void ss_stack_canary_init(uint16_t id) {
    if (id == 0 || id > SS_MAX_TASKS) return;
    SSTask* tcb = &tcb_table[id - 1];
    if (tcb->stack_size == 0) return;

    uint8_t* bottom = (uint8_t*)((uintptr_t)tcb->stack_base - tcb->stack_size + 4);
    for (uint32_t offset = 0; offset < tcb->stack_size; offset += 4) {
        *(uint32_t*)(bottom + offset) = STACK_CANARY;
    }
}

void ss_sched_init(void) {
    memset(tcb_table, 0, sizeof(tcb_table));
    memset(&ready_queue, 0, sizeof(ready_queue));
    ss_curr_task = NULL;
    ss_scheduled_task = NULL;
    ss_task_count = 0;
}

void ss_sched_enqueue(SSTask* tcb) {
    uint8_t pri = tcb->pri;
    if (pri >= SS_MAX_PRI) return;

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

void ss_sched_dequeue(SSTask* tcb) {
    uint8_t pri = tcb->pri;
    if (pri >= SS_MAX_PRI) return;

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

SSTask* ss_sched_pick(void) {
    if (ready_queue.pri_bitmap == 0) return NULL;

    uint16_t bits = ready_queue.pri_bitmap;
    int pri = 0;
    while (!(bits & 0x8000) && pri < 16) {
        bits <<= 1;
        pri++;
    }
    if (pri >= SS_MAX_PRI) return NULL;
    return ready_queue.heads[pri];
}

uint16_t ss_task_create(SSTaskInfo* info) {
    if (info == NULL || info->entry == NULL) return SS_ERR_PARAM;
    if (info->pri >= SS_MAX_PRI) return SS_ERR_PARAM;

    ss_disable_interrupts();

    uint16_t i;
    for (i = 0; i < SS_MAX_TASKS; i++) {
        if (tcb_table[i].state == SS_TS_NONE) break;
    }
    if (i >= SS_MAX_TASKS) {
        ss_enable_interrupts();
        return SS_ERR_LIMIT;
    }

    SSTask* tcb = &tcb_table[i];
    memset(tcb, 0, sizeof(SSTask));
    tcb->state = SS_TS_DORMANT;
    tcb->entry = info->entry;
    tcb->pri = info->pri;
    tcb->ctx_level = info->ctx_level;

    if (info->stack != NULL) {
        tcb->stack_base = info->stack;
        tcb->stack_size = info->stack_size;
    } else {
        extern uint8_t* ss_task_stack_base;
        if (ss_task_stack_base == NULL) {
            ss_enable_interrupts();
            return SS_ERR_STATE;
        }
        uintptr_t stack_end = (uintptr_t)(ss_task_stack_base + (i + 1) * SS_TASK_STACK);
        tcb->stack_base = (uint8_t*)((stack_end - 4) & ~(uintptr_t)3);
        tcb->stack_size = SS_TASK_STACK;
    }

    tcb->context = tcb->stack_base;

    ss_stack_canary_init(i + 1);

    ss_task_count++;

    ss_enable_interrupts();
    return i + 1;  /* 1-based ID */
}

uint16_t ss_task_start(uint16_t id) {
    if (id == 0 || id > SS_MAX_TASKS) return SS_ERR_ID;

    ss_disable_interrupts();
    SSTask* tcb = &tcb_table[id - 1];

    if (tcb->state != SS_TS_DORMANT) {
        ss_enable_interrupts();
        return SS_ERR_STATE;
    }

    tcb->state = SS_TS_READY;
    ss_sched_enqueue(tcb);

    if (ss_curr_task == NULL) {
        ss_curr_task = tcb;
    }

    ss_enable_interrupts();
    return SS_OK;
}

void ss_do_context_switch(void) {
    SSTask* curr = ss_curr_task;
    if (curr == NULL) return;

    /* Round-robin: move current task to tail BEFORE picking next.
       This ensures ss_sched_pick() returns a different task. */
    if (curr->state == SS_TS_READY) {
        ss_sched_dequeue(curr);
        ss_sched_enqueue(curr);
    }

    SSTask* next = ss_sched_pick();
    if (next == NULL || next == curr) {
        ss_scheduled_task = curr;
        return;
    }

    ss_scheduled_task = next;
    ss_curr_task = next;
}

uint16_t ss_task_sleep(uint32_t ticks) {
    SSTask* curr = ss_curr_task;
    if (curr == NULL) return SS_ERR_STATE;

    ss_disable_interrupts();
    curr->state = SS_TS_WAIT;
    curr->wait_until = ss_tick_counter + ticks;
    ss_sched_dequeue(curr);
    /* Yield immediately — rte restores interrupts (SR=0x2000) */
    ss_task_yield();
    return SS_OK;
}

void ss_do_wakeups(void) {
    if (ss_task_count == 0) return;
    uint32_t now = ss_tick_counter;
    uint16_t checked = 0;
    for (uint16_t i = 0; i < SS_MAX_TASKS && checked < ss_task_count; i++) {
        SSTask* tcb = &tcb_table[i];
        if (tcb->state == SS_TS_NONE) continue;
        checked++;
        if (tcb->state == SS_TS_WAIT && tcb->wait_until <= now) {
            tcb->state = SS_TS_READY;
            tcb->wait_until = 0;
            ss_sched_enqueue(tcb);
        }
    }
}

void ss_process_wakeups(void) {
    ss_do_wakeups();
}
