#include "ipc.h"
#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include <string.h>

static SSMsgQueue msg_queues[SS_MAX_TASKS];

void ss_ipc_init(void) {
    memset(msg_queues, 0, sizeof(msg_queues));
}

int16_t ss_send(uint16_t target, SSMessage* msg) {
    if (target == 0 || target > SS_MAX_TASKS) return SS_ERR_ID;
    if (msg == NULL) return SS_ERR_PARAM;

    SSMsgQueue* q = &msg_queues[target - 1];

    ss_disable_interrupts();
    if (q->count >= SS_MSG_MAX) {
        ss_enable_interrupts();
        return SS_ERR_LIMIT;
    }

    SSMessage* slot = &q->msgs[q->tail];
    memcpy(slot, msg, sizeof(SSMessage));
    slot->receiver = target;

    q->tail = (q->tail + 1) % SS_MSG_MAX;
    q->count++;
    ss_enable_interrupts();

    return SS_OK;
}

int16_t ss_recv(SSMessage* msg) {
    if (msg == NULL) return SS_ERR_PARAM;

    /* Determine current task's queue */
    SSTask* curr = ss_curr_task;
    if (curr == NULL) return SS_ERR_STATE;

    uint16_t id = (uint16_t)((curr - tcb_table) + 1);
    SSMsgQueue* q = &msg_queues[id - 1];

    /* Block until message available */
    while (q->count == 0) {
        ss_task_yield();
    }

    ss_disable_interrupts();
    if (q->count == 0) {
        ss_enable_interrupts();
        return SS_ERR_STATE;
    }

    memcpy(msg, &q->msgs[q->head], sizeof(SSMessage));
    q->head = (q->head + 1) % SS_MSG_MAX;
    q->count--;
    ss_enable_interrupts();

    return SS_OK;
}

int16_t ss_recv_nb(SSMessage* msg) {
    if (msg == NULL) return SS_ERR_PARAM;

    SSTask* curr = ss_curr_task;
    if (curr == NULL) return SS_ERR_STATE;

    uint16_t id = (uint16_t)((curr - tcb_table) + 1);
    SSMsgQueue* q = &msg_queues[id - 1];

    if (q->count == 0) return SS_ERR_LIMIT;

    ss_disable_interrupts();
    memcpy(msg, &q->msgs[q->head], sizeof(SSMessage));
    q->head = (q->head + 1) % SS_MSG_MAX;
    q->count--;
    ss_enable_interrupts();

    return SS_OK;
}
