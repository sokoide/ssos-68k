/* test_scheduler.c - scheduler queue / task lifecycle / sleep-wakeup tests.
 *
 * Learning objective: the scheduler's C logic (priority bitmap queue, task
 * state machine, timed wakeups) is HW-independent; only the interrupt guards,
 * tick source, and context-switch asm touch HW. Those are stubbed in
 * test_mocks.c, so the same scheduler.c we ship runs here. The stub does NOT
 * swap register state — ss_task_yield just drives the queue rotation — so we
 * test state transitions, not true concurrency.
 *
 * Built once per SCHED= variant: cooperative and preemptive share this file
 * because their queue/task/wakeup logic is identical. */

#include "ssos_test.h"
#include "scheduler.h"
#include "kernel.h"

/* ss_tick_counter is defined in test_mocks.c; bump it to simulate ticks. */
#define ADVANCE_TICK(n) (ss_tick_counter += (n))

static void* dummy_entry(void* arg) { (void)arg; return NULL; }

/* Create a task at the given priority using the auto stack arena. */
static uint16_t make_task(uint8_t pri) {
    SSTaskInfo info = {
        .entry = dummy_entry,
        .pri = pri,
        .ctx_level = SS_CTX_NORMAL,
        .stack_size = SS_TASK_STACK,
        .stack = NULL,            /* auto-allocate from ss_task_stack_base */
    };
    return ss_task_create(&info);
}

/* ---- queue primitives ---- */

TEST(sched_init_clears_state) {
    ss_sched_init();
    ASSERT_EQ(ready_queue.pri_bitmap, 0);
    ASSERT_NULL(ss_curr_task);
    ASSERT_NULL(ss_scheduled_task);
}

TEST(pick_empty_returns_null) {
    ss_sched_init();
    ASSERT_NULL(ss_sched_pick());
}

TEST(enqueue_dequeue_single_pri) {
    ss_sched_init();
    SSTask a = { .pri = 5 }, b = { .pri = 5 };
    ss_sched_enqueue(&a);
    ss_sched_enqueue(&b);
    /* FIFO: head=a, tail=b. */
    ASSERT_EQ(ready_queue.heads[5], &a);
    ASSERT_EQ(ready_queue.tails[5], &b);
    ASSERT_NEQ(ready_queue.pri_bitmap & (1 << (15 - 5)), 0);

    ASSERT_EQ(ss_sched_pick(), &a);   /* head of highest set pri */

    ss_sched_dequeue(&a);
    ASSERT_EQ(ready_queue.heads[5], &b);
    ss_sched_dequeue(&b);
    ASSERT_EQ(ready_queue.pri_bitmap, 0);   /* bit cleared when empty */
}

TEST(pick_prefers_lower_pri_number) {
    ss_sched_init();
    SSTask hi = { .pri = 0 };   /* pri 0 = highest (bit 15) */
    SSTask lo = { .pri = 10 };
    ss_sched_enqueue(&lo);
    ss_sched_enqueue(&hi);
    ASSERT_EQ(ss_sched_pick(), &hi);
}

/* ---- task lifecycle ---- */

TEST(task_create_null_entry_rejected) {
    ss_sched_init();
    SSTaskInfo info = { .entry = NULL, .pri = 1, .stack_size = SS_TASK_STACK, .stack = NULL };
    ASSERT_EQ(ss_task_create(&info), (uint16_t)SS_ERR_PARAM);
}

TEST(task_create_bad_pri_rejected) {
    ss_sched_init();
    SSTaskInfo info = { .entry = dummy_entry, .pri = SS_MAX_PRI, .stack_size = SS_TASK_STACK, .stack = NULL };
    ASSERT_EQ(ss_task_create(&info), (uint16_t)SS_ERR_PARAM);
}

TEST(task_create_returns_ascending_ids) {
    ss_sched_init();
    uint16_t a = make_task(1);
    uint16_t b = make_task(1);
    ASSERT_EQ(a, 1);
    ASSERT_EQ(b, 2);
    ASSERT_EQ(tcb_table[a - 1].state, SS_TS_DORMANT);
    ASSERT_EQ(tcb_table[a - 1].pri, 1);
}

TEST(task_create_exhaustion) {
    ss_sched_init();
    for (int i = 0; i < SS_MAX_TASKS; i++) {
        ASSERT_EQ(make_task(1), (uint16_t)(i + 1));
    }
    /* Table full -> SS_ERR_LIMIT. */
    ASSERT_EQ(make_task(1), (uint16_t)SS_ERR_LIMIT);
}

TEST(task_start_moves_to_ready) {
    ss_sched_init();
    uint16_t id = make_task(1);
    ASSERT_EQ(ss_task_start(id), (uint16_t)SS_OK);
    ASSERT_EQ(tcb_table[id - 1].state, SS_TS_READY);
    /* First started task becomes current. */
    ASSERT_EQ(ss_curr_task, &tcb_table[id - 1]);
    ASSERT_NEQ(ready_queue.pri_bitmap & (1 << (15 - 1)), 0);
}

TEST(task_start_double_rejected) {
    ss_sched_init();
    uint16_t id = make_task(1);
    ss_task_start(id);
    ASSERT_EQ(ss_task_start(id), (uint16_t)SS_ERR_STATE);
}

TEST(task_start_invalid_id_rejected) {
    ss_sched_init();
    ASSERT_EQ(ss_task_start(0), (uint16_t)SS_ERR_ID);
    ASSERT_EQ(ss_task_start(SS_MAX_TASKS + 1), (uint16_t)SS_ERR_ID);
}

/* ---- context switch (round-robin within a priority) ---- */

TEST(context_switch_rotates_same_pri) {
    ss_sched_init();
    uint16_t a = make_task(1);
    uint16_t b = make_task(1);
    ss_task_start(a);
    ss_task_start(b);
    /* curr == a (first started). Both ready. */
    ASSERT_EQ(ss_curr_task, &tcb_table[a - 1]);
    ss_do_context_switch();
    /* a rotated to tail, b picked as next. */
    ASSERT_EQ(ss_curr_task, &tcb_table[b - 1]);
}

/* ---- sleep / wakeup ---- */

TEST(task_sleep_waits_and_wakes) {
    ss_sched_init();
    uint16_t a = make_task(1);
    ss_task_start(a);
    SSTask* t = &tcb_table[a - 1];

    /* Sleep 10 ticks: state -> WAIT, removed from ready queue. */
    ASSERT_EQ(ss_task_sleep(10), (uint16_t)SS_OK);
    ASSERT_EQ(t->state, SS_TS_WAIT);
    ASSERT_EQ(ready_queue.pri_bitmap, 0);

    /* Not yet elapsed: wakeups keeps it asleep. */
    ADVANCE_TICK(5);
    ss_do_wakeups();
    ASSERT_EQ(t->state, SS_TS_WAIT);

    /* Elapsed: reaped back to READY and re-enqueued. */
    ADVANCE_TICK(5);   /* now == 10 == wait_until */
    ss_do_wakeups();
    ASSERT_EQ(t->state, SS_TS_READY);
    ASSERT_NEQ(ready_queue.pri_bitmap & (1 << (15 - 1)), 0);
}

TEST(task_sleep_no_current_fails) {
    ss_sched_init();
    ASSERT_EQ(ss_task_sleep(5), (uint16_t)SS_ERR_STATE);
}

void run_scheduler_tests(void) {
    RUN_TEST(sched_init_clears_state);
    RUN_TEST(pick_empty_returns_null);
    RUN_TEST(enqueue_dequeue_single_pri);
    RUN_TEST(pick_prefers_lower_pri_number);
    RUN_TEST(task_create_null_entry_rejected);
    RUN_TEST(task_create_bad_pri_rejected);
    RUN_TEST(task_create_returns_ascending_ids);
    RUN_TEST(task_create_exhaustion);
    RUN_TEST(task_start_moves_to_ready);
    RUN_TEST(task_start_double_rejected);
    RUN_TEST(task_start_invalid_id_rejected);
    RUN_TEST(context_switch_rotates_same_pri);
    RUN_TEST(task_sleep_waits_and_wakes);
    RUN_TEST(task_sleep_no_current_fails);
}
