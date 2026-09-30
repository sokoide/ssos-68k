#ifndef SS_WORK_QUEUE_H
#define SS_WORK_QUEUE_H

#include <stdint.h>

#define SS_WORK_QUEUE_SIZE 32

typedef struct {
    void (*handler)(void* arg);
    void* arg;
    uint32_t posted_tick;
} SSWorkItem;

typedef struct {
    SSWorkItem items[SS_WORK_QUEUE_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
    uint16_t high_water;
    uint32_t rejected;
    uint32_t max_delay_ticks;
} SSWorkQueue;

void     ss_work_init(SSWorkQueue* q);
int16_t  ss_work_enqueue(SSWorkQueue* q, void (*handler)(void*), void* arg);
/* Single consumer. arg remains producer-owned until its callback completes.
 * A failed enqueue does not transfer ownership. IRQ callers may enqueue only
 * persistent payloads; handlers execute in the consumer task, outside the lock.
 * Handlers must return promptly; recursive drain is not supported.
 * drain handles only the initial batch, capped by budget, never newly posted work.
 */
uint16_t ss_work_drain_budget(SSWorkQueue* q, uint16_t budget);
void     ss_work_drain(SSWorkQueue* q);

extern SSWorkQueue ss_main_work_queue;

#endif /* SS_WORK_QUEUE_H */
