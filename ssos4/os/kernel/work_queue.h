#ifndef S4_WORK_QUEUE_H
#define S4_WORK_QUEUE_H

#include <stdint.h>

#define S4_WORK_QUEUE_SIZE 32

typedef struct {
    void (*handler)(void* arg);
    void* arg;
} S4WorkItem;

typedef struct {
    S4WorkItem items[S4_WORK_QUEUE_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} S4WorkQueue;

void     s4_work_init(S4WorkQueue* q);
int16_t  s4_work_enqueue(S4WorkQueue* q, void (*handler)(void*), void* arg);
void     s4_work_drain(S4WorkQueue* q);

extern S4WorkQueue s4_main_work_queue;

#endif /* S4_WORK_QUEUE_H */
