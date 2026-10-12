/* t_ipc_samepri.c - blocking ss_recv between equal-priority tasks.
 *
 * t_ipc_blocking covers a high-pri receiver starved of a low-pri sender.
 * The other R04 combination is receiver and sender at the SAME priority:
 * blocking must dequeue the receiver so round-robin reaches the sender,
 * and the wake must hand control back per queue order, not priority.
 */

#include "status.h"

#include "scheduler.h"
#include "main_task.h"
#include "ipc.h"
#include "tty.h"

static SSTask main_tcb;
static volatile uint16_t received;

#define IPC_ROUNDS 32

static void fail(const char* reason) {
    tty_puts("FAIL ");
    tty_puts(reason);
    tty_puts("\n");
    for (;;) {}
}

static void* receiver(void* arg) {
    (void)arg;
    SSMessage msg;
    for (uint16_t i = 0; i < IPC_ROUNDS; i++) {
        if (ss_recv(&msg) != SS_OK) fail("recv");
        if (msg.type != (uint16_t)(100 + i) || msg.receiver != 1)
            fail("message");
        /* Loop straight back into ss_recv: blocking is what hands the CPU
         * to the equal-priority sender and re-establishes the WAIT state
         * the sender checks on its next round. */
        received = (uint16_t)(i + 1);
    }
    tty_puts("OK ipc same-priority blocking\n");
    for (;;) ss_task_yield();
    return 0;
}

static void* sender(void* arg) {
    (void)arg;
    for (uint16_t i = 0; i < IPC_ROUNDS; i++) {
        if (tcb_table[0].state != SS_TS_WAIT || !tcb_table[0].ipc_waiting)
            fail("receiver did not block");
        SSMessage msg = {0};
        msg.type = (uint16_t)(100 + i);
        if (ss_send(1, &msg) != SS_OK) fail("send");
        while (received <= i) ss_task_yield();
    }
    for (;;) ss_task_yield();
    return 0;
}

int main(void) {
    ss_sched_init();
    ss_ipc_init();
    if (ss_main_task_register(&main_tcb, 15) != SS_OK)
        fail("register");

    SSTaskInfo recv_info = { .entry = receiver, .pri = 6 };
    SSTaskInfo send_info = { .entry = sender, .pri = 6 };
    uint16_t recv_id = ss_task_create(&recv_info);
    uint16_t send_id = ss_task_create(&send_info);
    if (recv_id != 1 || send_id != 2 ||
        ss_task_start(recv_id) != SS_OK ||
        ss_task_start(send_id) != SS_OK)
        fail("task setup");
    ss_task_yield();
    fail("not scheduled");
    return 0;
}
