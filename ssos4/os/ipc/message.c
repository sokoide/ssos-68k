#include "ipc.h"
#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include <string.h>

static S4MsgQueue msg_queues[S4_MAX_TASKS];

void s4_ipc_init(void) {
    memset(msg_queues, 0, sizeof(msg_queues));
}

int16_t s4_send(uint16_t target, S3Message* msg) {
    if (target == 0 || target > S4_MAX_TASKS) return S4_ERR_ID;
    if (msg == NULL) return S4_ERR_PARAM;

    S4MsgQueue* q = &msg_queues[target - 1];

    s4_disable_interrupts();
    if (q->count >= S4_MSG_MAX) {
        s4_enable_interrupts();
        return S4_ERR_LIMIT;
    }

    S3Message* slot = &q->msgs[q->tail];
    memcpy(slot, msg, sizeof(S3Message));
    slot->receiver = target;

    q->tail = (q->tail + 1) % S4_MSG_MAX;
    q->count++;
    s4_enable_interrupts();

    return S4_OK;
}

int16_t s4_recv(S3Message* msg) {
    if (msg == NULL) return S4_ERR_PARAM;

    /* Determine current task's queue */
    S4Task* curr = (S4Task*)s4_curr_task;
    if (curr == NULL) return S4_ERR_STATE;

    uint16_t id = (uint16_t)((curr - tcb_table) + 1);
    S4MsgQueue* q = &msg_queues[id - 1];

    /* Block until message available */
    while (q->count == 0) {
        s4_task_yield();
    }

    s4_disable_interrupts();
    if (q->count == 0) {
        s4_enable_interrupts();
        return S4_ERR_STATE;
    }

    memcpy(msg, &q->msgs[q->head], sizeof(S3Message));
    q->head = (q->head + 1) % S4_MSG_MAX;
    q->count--;
    s4_enable_interrupts();

    return S4_OK;
}

int16_t s4_recv_nb(S3Message* msg) {
    if (msg == NULL) return S4_ERR_PARAM;

    S4Task* curr = (S4Task*)s4_curr_task;
    if (curr == NULL) return S4_ERR_STATE;

    uint16_t id = (uint16_t)((curr - tcb_table) + 1);
    S4MsgQueue* q = &msg_queues[id - 1];

    if (q->count == 0) return S4_ERR_LIMIT;

    s4_disable_interrupts();
    memcpy(msg, &q->msgs[q->head], sizeof(S3Message));
    q->head = (q->head + 1) % S4_MSG_MAX;
    q->count--;
    s4_enable_interrupts();

    return S4_OK;
}
