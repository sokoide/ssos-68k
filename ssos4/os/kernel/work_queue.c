#include "work_queue.h"
#include "kernel.h"
#include <string.h>

S4WorkQueue s4_main_work_queue;

void s4_work_init(S4WorkQueue* q) {
    memset(q, 0, sizeof(S4WorkQueue));
}

int16_t s4_work_enqueue(S4WorkQueue* q, void (*handler)(void*), void* arg) {
    if (q->count >= S4_WORK_QUEUE_SIZE) return S4_ERR_LIMIT;

    s4_disable_interrupts();

    S4WorkItem* item = &q->items[q->tail];
    item->handler = handler;
    item->arg = arg;

    q->tail = (q->tail + 1) % S4_WORK_QUEUE_SIZE;
    q->count++;

    s4_enable_interrupts();
    return S4_OK;
}

void s4_work_drain(S4WorkQueue* q) {
    while (q->count > 0) {
        s4_disable_interrupts();
        if (q->count == 0) {
            s4_enable_interrupts();
            break;
        }

        S4WorkItem item = q->items[q->head];
        q->head = (q->head + 1) % S4_WORK_QUEUE_SIZE;
        q->count--;
        s4_enable_interrupts();

        if (item.handler) {
            item.handler(item.arg);
        }
    }
}
