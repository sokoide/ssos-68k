#include "scheduler.h"
#include "main_task.h"
#include "ipc.h"

extern void tty_puts(const char*);

static SSTask main_tcb;
static volatile int sent;

static void fail(const char* reason) {
    tty_puts("FAIL ");
    tty_puts(reason);
    tty_puts("\n");
    for (;;) {}
}

static uint16_t read_sr(void) {
    uint16_t sr;
    __asm__ volatile ("move.w %%sr,%0" : "=d"(sr));
    return sr;
}

static void* receiver(void* arg) {
    (void)arg;
    SSMessage msg;
    if (ss_recv(&msg) != SS_OK) fail("recv");
    if (!sent || msg.type != 42 || msg.receiver != 1)
        fail("message");
    tty_puts("OK ipc blocking + irq restore\n");
    for (;;) {}
    return 0;
}

static void* sender(void* arg) {
    (void)arg;
    if (tcb_table[0].state != SS_TS_WAIT || !tcb_table[0].ipc_waiting)
        fail("receiver did not block");
    SSMessage msg = {0};
    msg.type = 42;
    if (ss_send(1, &msg) != SS_OK) fail("send");
    sent = 1;
    for (;;) ss_task_yield();
    return 0;
}

int main(void) {
    ss_sched_init();
    ss_ipc_init();
    if (ss_main_task_register(&main_tcb, 15) != SS_OK)
        fail("register");
    SSMessage msg;
    if (ss_recv_nb(&msg) != SS_ERR_STATE || ss_recv(&msg) != SS_ERR_STATE)
        fail("main queue");

    uint16_t initial = read_sr();
    uint16_t outer = ss_irq_save();
    if ((read_sr() & 0x0700) != 0x0700) fail("outer mask");
    uint16_t inner = ss_irq_save();
    ss_irq_restore(inner);
    if ((read_sr() & 0x0700) != 0x0700) fail("nested mask");
    ss_irq_restore(outer);
    if ((read_sr() & 0x0700) != (initial & 0x0700))
        fail("restore mask");

    SSTaskInfo recv_info = { .entry = receiver, .pri = 1 };
    SSTaskInfo send_info = { .entry = sender, .pri = 10 };
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
