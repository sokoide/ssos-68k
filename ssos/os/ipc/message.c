#include "ipc.h"
#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include <string.h>

static SSMsgQueue msg_queues[SS_MAX_TASKS];

/* The bootstrap main task has no queue or task ID. Compare actual TCB
 * addresses so no pointer subtraction is performed on unrelated objects. */
static int current_queue_index(void) {
    for (int i = 0; i < SS_MAX_TASKS; i++) {
        if (ss_curr_task == &tcb_table[i]) return i;
    }
    return -1;
}

void ss_ipc_init(void) {
    memset(msg_queues, 0, sizeof(msg_queues));
}

int16_t ss_send(uint16_t target, SSMessage* msg) {
    if (target == 0 || target > SS_MAX_TASKS) return SS_ERR_ID;
    if (msg == NULL) return SS_ERR_PARAM;

    SSMsgQueue* q = &msg_queues[target - 1];

    uint16_t saved_sr = ss_irq_save();
    if (q->count >= SS_MSG_MAX) {
        ss_irq_restore(saved_sr);
        return SS_ERR_LIMIT;
    }

    SSMessage* slot = &q->msgs[q->tail];
    memcpy(slot, msg, sizeof(SSMessage));
    slot->receiver = target;

    q->tail = (q->tail + 1) % SS_MSG_MAX;
    q->count++;
    SSTask* receiver = &tcb_table[target - 1];
    if (receiver->state == SS_TS_WAIT && receiver->ipc_waiting) {
        receiver->ipc_waiting = 0;
        receiver->state = SS_TS_READY;
        ss_sched_enqueue(receiver);
    }
    ss_irq_restore(saved_sr);

    return SS_OK;
}

int16_t ss_recv(SSMessage* msg) {
    if (msg == NULL) return SS_ERR_PARAM;

    uint16_t saved_sr = ss_irq_save();
    int index = current_queue_index();
    if (index < 0) {
        ss_irq_restore(saved_sr);
        return SS_ERR_STATE;
    }
    SSTask* curr = ss_curr_task;
    SSMsgQueue* q = &msg_queues[index];

    while (q->count == 0) {
        if (curr->state != SS_TS_READY ||
            (ready_queue.pri_bitmap == (uint16_t)(1u << (15 - curr->pri)) &&
             ready_queue.heads[curr->pri] == curr &&
             ready_queue.tails[curr->pri] == curr)) {
            ss_irq_restore(saved_sr);
            return SS_ERR_STATE;
        }
        curr->ipc_waiting = 1;
        curr->state = SS_TS_WAIT;
        ss_sched_dequeue(curr);
        ss_task_yield();
    }

    memcpy(msg, &q->msgs[q->head], sizeof(SSMessage));
    q->head = (q->head + 1) % SS_MSG_MAX;
    q->count--;
    ss_irq_restore(saved_sr);

    return SS_OK;
}

int16_t ss_recv_nb(SSMessage* msg) {
    if (msg == NULL) return SS_ERR_PARAM;

    uint16_t saved_sr = ss_irq_save();
    int index = current_queue_index();
    if (index < 0) {
        ss_irq_restore(saved_sr);
        return SS_ERR_STATE;
    }
    SSMsgQueue* q = &msg_queues[index];
    if (q->count == 0) {
        ss_irq_restore(saved_sr);
        return SS_ERR_LIMIT;
    }
    memcpy(msg, &q->msgs[q->head], sizeof(SSMessage));
    q->head = (q->head + 1) % SS_MSG_MAX;
    q->count--;
    ss_irq_restore(saved_sr);

    return SS_OK;
}
