#include "status.h"
#include "irq.h"
#include "clock.h"
#include "work_queue.h"
#include <string.h>

SSWorkQueue ss_main_work_queue;

void ss_work_init(SSWorkQueue* q) {
    memset(q, 0, sizeof(SSWorkQueue));
}

int16_t ss_work_enqueue(SSWorkQueue* q, void (*handler)(void*), void* arg) {
    if (q == NULL || handler == NULL) return SS_ERR_PARAM;
    uint16_t saved_sr = ss_irq_save();
    if (q->count >= SS_WORK_QUEUE_SIZE) {
        q->rejected++;
        ss_irq_restore(saved_sr);
        return SS_ERR_LIMIT;
    }

    SSWorkItem* item = &q->items[q->tail];
    item->handler = handler;
    item->arg = arg;
    item->posted_tick = ss_tick_counter;

    q->tail = (q->tail + 1) % SS_WORK_QUEUE_SIZE;
    q->count++;
    if (q->count > q->high_water) q->high_water = q->count;

    ss_irq_restore(saved_sr);
    return SS_OK;
}

uint16_t ss_work_drain_budget(SSWorkQueue* q, uint16_t budget) {
    if (q == NULL) return 0;
    uint16_t saved_sr = ss_irq_save();
    uint16_t batch = q->count < budget ? q->count : budget;
    ss_irq_restore(saved_sr);
    uint16_t done = 0;
    while (done < batch) {
        saved_sr = ss_irq_save();
        SSWorkItem item = q->items[q->head];
        q->head = (q->head + 1) % SS_WORK_QUEUE_SIZE;
        q->count--;
        uint32_t delay = ss_tick_counter - item.posted_tick;
        if (delay > q->max_delay_ticks) q->max_delay_ticks = delay;
        ss_irq_restore(saved_sr);
        item.handler(item.arg);
        done++;
    }
    return done;
}

void ss_work_drain(SSWorkQueue* q) {
    (void)ss_work_drain_budget(q, SS_WORK_QUEUE_SIZE);
}
