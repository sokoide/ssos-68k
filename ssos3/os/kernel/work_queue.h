#ifndef S3_WORK_QUEUE_H
#define S3_WORK_QUEUE_H

#include <stdint.h>

#define S3_WORK_QUEUE_SIZE 32

typedef struct {
    void (*handler)(void* arg);
    void* arg;
} S3WorkItem;

typedef struct {
    S3WorkItem items[S3_WORK_QUEUE_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} S3WorkQueue;

void     s3_work_init(S3WorkQueue* q);
int16_t  s3_work_enqueue(S3WorkQueue* q, void (*handler)(void*), void* arg);
void     s3_work_drain(S3WorkQueue* q);

extern S3WorkQueue s3_main_work_queue;

#endif /* S3_WORK_QUEUE_H */
