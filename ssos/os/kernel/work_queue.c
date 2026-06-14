#include "work_queue.h"
#include "kernel.h"
#include <string.h>

SSWorkQueue ss_main_work_queue;

void ss_work_init(SSWorkQueue* q) {
    memset(q, 0, sizeof(SSWorkQueue));
}

int16_t ss_work_enqueue(SSWorkQueue* q, void (*handler)(void*), void* arg) {
    if (q->count >= SS_WORK_QUEUE_SIZE) return SS_ERR_LIMIT;

    ss_disable_interrupts();

    SSWorkItem* item = &q->items[q->tail];
    item->handler = handler;
    item->arg = arg;

    q->tail = (q->tail + 1) % SS_WORK_QUEUE_SIZE;
    q->count++;

    ss_enable_interrupts();
    return SS_OK;
}

void ss_work_drain(SSWorkQueue* q) {
    while (q->count > 0) {
        ss_disable_interrupts();
        if (q->count == 0) {
            ss_enable_interrupts();
            break;
        }

        SSWorkItem item = q->items[q->head];
        q->head = (q->head + 1) % SS_WORK_QUEUE_SIZE;
        q->count--;
        ss_enable_interrupts();

        if (item.handler) {
            item.handler(item.arg);
        }
    }
}
