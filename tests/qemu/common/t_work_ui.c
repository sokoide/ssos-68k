#include "status.h"
#include "main_task.h"
#include "work_queue.h"
#include "tty.h"
static SSTask main_tcb;
static SSWorkQueue queue;
static unsigned payload[32], handled;
static void fail(void) { tty_puts("FAIL UI work ownership/budget\n"); for (;;) {} }
static void apply(void* arg) {
    if (ss_curr_task != &main_tcb || *(unsigned*)arg != handled) fail();
    handled++;
}
static void* producer(void* arg) {
    (void)arg;
    for (unsigned i = 0; i < 32; i++) {
        payload[i] = i;
        if (ss_work_enqueue(&queue, apply, &payload[i]) != SS_OK) fail();
    }
    if (ss_work_enqueue(&queue, apply, &payload[0]) != SS_ERR_LIMIT || handled) fail();
    return NULL;
}
int main(void) {
    ss_sched_init(); ss_work_init(&queue);
    ss_main_task_register(&main_tcb, 15);
    SSTaskInfo info = {.entry = producer, .pri = 1};
    uint16_t id = ss_task_create(&info);
    ss_task_start(id); ss_task_yield();
    for (unsigned frame = 0; frame < 8; frame++) {
        if (ss_work_drain_budget(&queue, 4) != 4 || handled != (frame + 1) * 4) fail();
        ss_task_yield();
    }
    if (queue.count || queue.rejected != 1 || queue.high_water != 32) fail();
    tty_puts("OK worker to UI persistent payload + full queue + frame budget\n");
    for (;;) {}
}
