#include "status.h"
#include "clock.h"
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

static SSWorkQueue recurrent;
static void repost(void* arg) {
    (void)arg;
    handled_count++;
    (void)ss_work_enqueue(&recurrent, repost, NULL);
}
TEST(work_reposting_leaves_next_batch_pending) {
    ss_work_init(&recurrent);
    handled_count = 0;
    ASSERT_EQ(ss_work_enqueue(&recurrent, repost, NULL), SS_OK);
    ss_work_drain(&recurrent);
    ASSERT_EQ(handled_count, 1);
    ASSERT_EQ(recurrent.count, 1);
    ASSERT_EQ(ss_work_drain_budget(&recurrent, 0), 0);
    ASSERT_EQ(ss_work_drain_budget(&recurrent, 1), 1);
    ASSERT_EQ(recurrent.high_water, 1);
}
TEST(work_delay_ticks_wrap) {
    SSWorkQueue q;
    ss_work_init(&q); handled_count = 0;
    ss_tick_counter = 0xfffffffeu;
    ASSERT_EQ(ss_work_enqueue(&q, record_handler, NULL), SS_OK);
    ss_tick_counter = 3;
    ss_work_drain(&q);
    ASSERT_EQ(q.max_delay_ticks, 5);
}
TEST(work_budget_and_invalid_requests) {
    SSWorkQueue q;
    ss_work_init(&q);
    handled_count = 0;
    ASSERT_EQ(ss_work_enqueue(NULL, record_handler, NULL), SS_ERR_PARAM);
    ASSERT_EQ(ss_work_enqueue(&q, NULL, NULL), SS_ERR_PARAM);
    for (int i = 0; i < SS_WORK_QUEUE_SIZE; i++)
        ASSERT_EQ(ss_work_enqueue(&q, record_handler, (void*)(intptr_t)i), SS_OK);
    ASSERT_EQ(ss_work_enqueue(&q, record_handler, NULL), SS_ERR_LIMIT);
    ASSERT_EQ(q.rejected, 1);
    ASSERT_EQ(q.high_water, SS_WORK_QUEUE_SIZE);
    ASSERT_EQ(ss_work_drain_budget(&q, 2), 2);
    ASSERT_EQ(q.count, SS_WORK_QUEUE_SIZE - 2);
}
void run_work_queue_tests(void) {
    RUN_TEST(work_reposting_leaves_next_batch_pending);
    RUN_TEST(work_budget_and_invalid_requests);
    RUN_TEST(work_delay_ticks_wrap);
    RUN_TEST(work_queue_init_is_empty);
    RUN_TEST(work_queue_drains_in_fifo_order);
    RUN_TEST(work_queue_rejects_full_queue_without_corruption);
}
