#include "scheduler.h"
#include "main_task.h"
#include "ipc.h"

extern void tty_puts(const char*);

static SSTask main_tcb;
static volatile uint16_t received;

#define IPC_ROUNDS 32

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

static void write_ipl(uint16_t ipl) {
    uint16_t sr = read_sr();
    sr = (uint16_t)((sr & ~0x0700u) | (ipl << 8));
    __asm__ volatile ("move.w %0,%%sr" : : "d"(sr) : "memory");
}

static void* receiver(void* arg) {
    (void)arg;
    SSMessage msg;
    for (uint16_t i = 0; i < IPC_ROUNDS; i++) {
        if (ss_recv(&msg) != SS_OK) fail("recv");
        if (msg.type != (uint16_t)(42 + i) || msg.receiver != 1)
            fail("message");
        received = (uint16_t)(i + 1);
    }
    tty_puts("OK ipc repeated blocking + irq restore\n");
    for (;;) {}
    return 0;
}

static void* sender(void* arg) {
    (void)arg;
    for (uint16_t i = 0; i < IPC_ROUNDS; i++) {
        if (tcb_table[0].state != SS_TS_WAIT || !tcb_table[0].ipc_waiting)
            fail("receiver did not block");
        SSMessage msg = {0};
        msg.type = (uint16_t)(42 + i);
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
    SSMessage msg;
    if (ss_recv_nb(&msg) != SS_ERR_STATE || ss_recv(&msg) != SS_ERR_STATE)
        fail("main queue");

    uint16_t original_sr = read_sr();
    for (uint16_t ipl = 0; ipl <= 7; ipl++) {
        write_ipl(ipl);
        uint16_t expected = (uint16_t)(read_sr() & 0xFF00u);
        uint16_t outer = ss_irq_save();
        if ((read_sr() & 0x0700) != 0x0700) fail("outer mask");
        uint16_t inner = ss_irq_save();
        ss_irq_restore(inner);
        if ((read_sr() & 0x0700) != 0x0700) fail("nested mask");
        ss_irq_restore(outer);
        if ((read_sr() & 0xFF00u) != expected) fail("restore mask");
    }
    __asm__ volatile ("move.w %0,%%sr" : : "d"(original_sr) : "memory");

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
