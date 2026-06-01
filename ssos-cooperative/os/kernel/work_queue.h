#ifndef SS_WORK_QUEUE_H
#define SS_WORK_QUEUE_H

#include <stdint.h>

#define SS_WORK_QUEUE_SIZE 32

typedef struct {
    void (*handler)(void* arg);
    void* arg;
} SSWorkItem;

typedef struct {
    SSWorkItem items[SS_WORK_QUEUE_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} SSWorkQueue;

void     ss_work_init(SSWorkQueue* q);
int16_t  ss_work_enqueue(SSWorkQueue* q, void (*handler)(void*), void* arg);
void     ss_work_drain(SSWorkQueue* q);

extern SSWorkQueue ss_main_work_queue;

#endif /* SS_WORK_QUEUE_H */
