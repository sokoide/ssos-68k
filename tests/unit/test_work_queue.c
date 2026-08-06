#include "kernel.h"
#include "ssos_test.h"
#include "work_queue.h"

static int handled[SS_WORK_QUEUE_SIZE];
static int handled_count;

static void record_handler(void* arg) {
    handled[handled_count++] = (int)(intptr_t)arg;
}

TEST(work_queue_init_is_empty) {
    SSWorkQueue q;
    ss_work_init(&q);

    ASSERT_EQ(q.head, 0);
    ASSERT_EQ(q.tail, 0);
    ASSERT_EQ(q.count, 0);
}

TEST(work_queue_drains_in_fifo_order) {
    SSWorkQueue q;
    ss_work_init(&q);
    handled_count = 0;

    ASSERT_EQ(ss_work_enqueue(&q, record_handler, (void*)(intptr_t)1), SS_OK);
    ASSERT_EQ(ss_work_enqueue(&q, record_handler, (void*)(intptr_t)2), SS_OK);
    ASSERT_EQ(ss_work_enqueue(&q, record_handler, (void*)(intptr_t)3), SS_OK);
    ss_work_drain(&q);

    ASSERT_EQ(handled_count, 3);
    ASSERT_EQ(handled[0], 1);
    ASSERT_EQ(handled[1], 2);
    ASSERT_EQ(handled[2], 3);
    ASSERT_EQ(q.count, 0);
}

TEST(work_queue_rejects_full_queue_without_corruption) {
    SSWorkQueue q;
    ss_work_init(&q);
    handled_count = 0;

    for (int i = 0; i < SS_WORK_QUEUE_SIZE; i++) {
        ASSERT_EQ(ss_work_enqueue(&q, record_handler, (void*)(intptr_t)i),
                  SS_OK);
    }
    ASSERT_EQ(ss_work_enqueue(&q, record_handler, NULL), SS_ERR_LIMIT);
    ASSERT_EQ(q.count, SS_WORK_QUEUE_SIZE);

    ss_work_drain(&q);
    ASSERT_EQ(handled_count, SS_WORK_QUEUE_SIZE);
    for (int i = 0; i < SS_WORK_QUEUE_SIZE; i++) {
        ASSERT_EQ(handled[i], i);
    }
}

void run_work_queue_tests(void) {
    RUN_TEST(work_queue_init_is_empty);
    RUN_TEST(work_queue_drains_in_fifo_order);
    RUN_TEST(work_queue_rejects_full_queue_without_corruption);
}
