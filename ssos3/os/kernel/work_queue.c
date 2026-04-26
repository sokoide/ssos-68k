#include "work_queue.h"
#include "kernel.h"
#include <string.h>

S3WorkQueue s3_main_work_queue;

void s3_work_init(S3WorkQueue* q) {
    memset(q, 0, sizeof(S3WorkQueue));
}

int16_t s3_work_enqueue(S3WorkQueue* q, void (*handler)(void*), void* arg) {
    if (q->count >= S3_WORK_QUEUE_SIZE) return S3_ERR_LIMIT;

    s3_disable_interrupts();

    S3WorkItem* item = &q->items[q->tail];
    item->handler = handler;
    item->arg = arg;

    q->tail = (q->tail + 1) % S3_WORK_QUEUE_SIZE;
    q->count++;

    s3_enable_interrupts();
    return S3_OK;
}

void s3_work_drain(S3WorkQueue* q) {
    while (q->count > 0) {
        s3_disable_interrupts();
        if (q->count == 0) {
            s3_enable_interrupts();
            break;
        }

        S3WorkItem item = q->items[q->head];
        q->head = (q->head + 1) % S3_WORK_QUEUE_SIZE;
        q->count--;
        s3_enable_interrupts();

        if (item.handler) {
            item.handler(item.arg);
        }
    }
}
