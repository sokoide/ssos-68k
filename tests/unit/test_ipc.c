/* test_ipc.c - tests for the inter-task message queue (os/ipc/message.c).
 *
 * Learning objective: message.c is pure logic over a ring buffer of SSMessage
 * slots, guarded by ss_disable/enable_interrupts (stubbed to no-ops in Native).
 * It depends on ss_curr_task + tcb_table to pick the receiver's queue — same
 * pattern as the scheduler — so we point ss_curr_task at tcb_table[0] (task id
 * 1) and exercise send / non-blocking recv / blocking recv / FIFO / wraparound.
 *
 * ss_recv blocks (yields) when the queue is empty, so we always pre-seed the
 * queue before calling it. */

#include "ssos_test.h"
#include "ipc.h"
#include "scheduler.h"
#include "kernel.h"

#include <string.h>

/* ss_curr_task points here so ss_recv* targets queue index 0 (task id 1). */
static void set_recv_task(int tcb_index) {
    ss_curr_task = &tcb_table[tcb_index];
}

static SSMessage make_msg(uint16_t type, uint16_t sender, const char* payload) {
    SSMessage m;
    memset(&m, 0, sizeof(m));
    m.type = type;
    m.sender = sender;
    if (payload) {
        size_t len = strlen(payload);
        if (len > SS_MSG_PAYLOAD) len = SS_MSG_PAYLOAD;
        memcpy(m.payload, payload, len);
        m.payload_size = (uint16_t)len;
    }
    return m;
}

/* ---- init ---- */

TEST(ipc_init_empty) {
    ss_ipc_init();
    set_recv_task(0);
    SSMessage out;
    ASSERT_EQ(ss_recv_nb(&out), (int16_t)SS_ERR_LIMIT);   /* queue empty */
}

/* ---- send / recv_nb round-trip ---- */

TEST(send_then_recv_nb) {
    ss_ipc_init();
    set_recv_task(0);

    SSMessage in = make_msg(7, 2, "hello");
    ASSERT_EQ(ss_send(1, &in), (int16_t)SS_OK);

    SSMessage out;
    ASSERT_EQ(ss_recv_nb(&out), (int16_t)SS_OK);
    ASSERT_EQ(out.type, 7);
    ASSERT_EQ(out.sender, 2);
    ASSERT_EQ(out.receiver, 1);             /* receiver stamped by ss_send */
    ASSERT_EQ(out.payload_size, 5);
    ASSERT_EQ(memcmp(out.payload, "hello", 5), 0);
}

TEST(send_invalid_id) {
    ss_ipc_init();
    SSMessage in = make_msg(1, 0, "x");
    ASSERT_EQ(ss_send(0, &in), (int16_t)SS_ERR_ID);
    ASSERT_EQ(ss_send(SS_MAX_TASKS + 1, &in), (int16_t)SS_ERR_ID);
}

TEST(send_null_msg) {
    ss_ipc_init();
    ASSERT_EQ(ss_send(1, NULL), (int16_t)SS_ERR_PARAM);
}

TEST(recv_nb_null) {
    ss_ipc_init();
    set_recv_task(0);
    ASSERT_EQ(ss_recv_nb(NULL), (int16_t)SS_ERR_PARAM);
}

TEST(recv_nb_no_current_task) {
    ss_ipc_init();
    ss_curr_task = NULL;
    SSMessage out;
    ASSERT_EQ(ss_recv_nb(&out), (int16_t)SS_ERR_STATE);
}

TEST(send_fills_to_limit) {
    ss_ipc_init();
    SSMessage in = make_msg(1, 0, "x");
    for (int i = 0; i < SS_MSG_MAX; i++) {
        ASSERT_EQ(ss_send(1, &in), (int16_t)SS_OK);
    }
    /* 65th message must be rejected (queue full). */
    ASSERT_EQ(ss_send(1, &in), (int16_t)SS_ERR_LIMIT);
}

/* ---- FIFO order ---- */

TEST(fifo_order) {
    ss_ipc_init();
    set_recv_task(0);

    SSMessage a = make_msg(10, 0, "AAA");
    SSMessage b = make_msg(20, 0, "BBB");
    SSMessage c = make_msg(30, 0, "CCC");
    ss_send(1, &a);
    ss_send(1, &b);
    ss_send(1, &c);

    SSMessage out;
    ss_recv_nb(&out);  ASSERT_EQ(out.type, 10);
    ss_recv_nb(&out);  ASSERT_EQ(out.type, 20);
    ss_recv_nb(&out);  ASSERT_EQ(out.type, 30);
}

/* ---- ring-buffer wraparound ---- */

TEST(tail_wraparound) {
    ss_ipc_init();
    set_recv_task(0);

    SSMessage in = make_msg(1, 0, "x");
    /* Fill the ring, then drain most of it so head/tail approach the end. */
    for (int i = 0; i < SS_MSG_MAX; i++) ASSERT_EQ(ss_send(1, &in), (int16_t)SS_OK);
    SSMessage out;
    for (int i = 0; i < SS_MSG_MAX - 1; i++) ASSERT_EQ(ss_recv_nb(&out), (int16_t)SS_OK);
    /* count==1, tail==SS_MSG_MAX%MAX==0 -> next send wraps tail to 0. */
    ASSERT_EQ(ss_send(1, &in), (int16_t)SS_OK);   /* now count==2 */
    ASSERT_EQ(ss_recv_nb(&out), (int16_t)SS_OK);
    ASSERT_EQ(ss_recv_nb(&out), (int16_t)SS_OK);
    ASSERT_EQ(ss_recv_nb(&out), (int16_t)SS_ERR_LIMIT);   /* empty again */
}

/* ---- blocking recv (pre-seeded so it doesn't block forever) ---- */

TEST(recv_returns_pre_seeded_message) {
    ss_ipc_init();
    set_recv_task(0);

    SSMessage in = make_msg(42, 9, "ping");
    ss_send(1, &in);

    SSMessage out;
    /* ss_recv blocks while empty, but the queue has one message so it returns
     * immediately (the ss_task_yield stub would just rotate an empty queue). */
    ASSERT_EQ(ss_recv(&out), (int16_t)SS_OK);
    ASSERT_EQ(out.type, 42);
    ASSERT_EQ(out.sender, 9);
}

/* ---- recv targets the current task's queue ---- */

TEST(recv_uses_current_task_queue) {
    ss_ipc_init();
    /* send to task id 3 (index 2), then recv as task 3 */
    SSMessage in = make_msg(5, 1, "to3");
    ASSERT_EQ(ss_send(3, &in), (int16_t)SS_OK);

    set_recv_task(2);   /* ss_curr_task -> tcb_table[2] -> task id 3 */
    SSMessage out;
    ASSERT_EQ(ss_recv_nb(&out), (int16_t)SS_OK);
    ASSERT_EQ(out.receiver, 3);

    /* task 1's queue must still be empty */
    set_recv_task(0);
    ASSERT_EQ(ss_recv_nb(&out), (int16_t)SS_ERR_LIMIT);
}

void run_ipc_tests(void) {
    RUN_TEST(ipc_init_empty);
    RUN_TEST(send_then_recv_nb);
    RUN_TEST(send_invalid_id);
    RUN_TEST(send_null_msg);
    RUN_TEST(recv_nb_null);
    RUN_TEST(recv_nb_no_current_task);
    RUN_TEST(send_fills_to_limit);
    RUN_TEST(fifo_order);
    RUN_TEST(tail_wraparound);
    RUN_TEST(recv_returns_pre_seeded_message);
    RUN_TEST(recv_uses_current_task_queue);
}
