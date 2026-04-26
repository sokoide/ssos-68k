#include "ipc.h"
#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include <string.h>

static S3MsgQueue msg_queues[S3_MAX_TASKS];

void s3_ipc_init(void) {
    memset(msg_queues, 0, sizeof(msg_queues));
}

int16_t s3_send(uint16_t target, S3Message* msg) {
    if (target == 0 || target > S3_MAX_TASKS) return S3_ERR_ID;
    if (msg == NULL) return S3_ERR_PARAM;

    S3MsgQueue* q = &msg_queues[target - 1];

    s3_disable_interrupts();
    if (q->count >= S3_MSG_MAX) {
        s3_enable_interrupts();
        return S3_ERR_LIMIT;
    }

    S3Message* slot = &q->msgs[q->tail];
    memcpy(slot, msg, sizeof(S3Message));
    slot->receiver = target;

    q->tail = (q->tail + 1) % S3_MSG_MAX;
    q->count++;
    s3_enable_interrupts();

    return S3_OK;
}

int16_t s3_recv(S3Message* msg) {
    if (msg == NULL) return S3_ERR_PARAM;

    /* Determine current task's queue */
    S3Task* curr = (S3Task*)s3_curr_task;
    if (curr == NULL) return S3_ERR_STATE;

    uint16_t id = (uint16_t)((curr - tcb_table) + 1);
    S3MsgQueue* q = &msg_queues[id - 1];

    /* Block until message available */
    while (q->count == 0) {
        s3_task_yield();
    }

    s3_disable_interrupts();
    if (q->count == 0) {
        s3_enable_interrupts();
        return S3_ERR_STATE;
    }

    memcpy(msg, &q->msgs[q->head], sizeof(S3Message));
    q->head = (q->head + 1) % S3_MSG_MAX;
    q->count--;
    s3_enable_interrupts();

    return S3_OK;
}

int16_t s3_recv_nb(S3Message* msg) {
    if (msg == NULL) return S3_ERR_PARAM;

    S3Task* curr = (S3Task*)s3_curr_task;
    if (curr == NULL) return S3_ERR_STATE;

    uint16_t id = (uint16_t)((curr - tcb_table) + 1);
    S3MsgQueue* q = &msg_queues[id - 1];

    if (q->count == 0) return S3_ERR_LIMIT;

    s3_disable_interrupts();
    memcpy(msg, &q->msgs[q->head], sizeof(S3Message));
    q->head = (q->head + 1) % S3_MSG_MAX;
    q->count--;
    s3_enable_interrupts();

    return S3_OK;
}
