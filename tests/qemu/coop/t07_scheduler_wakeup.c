#include "main_task.h"
#include "tty.h"

extern volatile uint8_t ss_wakeups_needed;

static SSTask main_tcb;
static volatile int worker_ran;

static void* worker(void* arg) {
    (void)arg;
    worker_ran = 1;
    ss_tick_counter += 3;
    ss_wakeups_needed = 1;
    ss_task_yield();
    for (;;) ss_task_yield();
    return 0;
}

int main(void) {
    ss_sched_init();
    if (ss_main_task_register(&main_tcb, 10) != SS_OK) {
        tty_puts("FAIL register\n");
        for (;;) {}
    }
    SSTaskInfo info = { .entry = worker, .pri = 15 };
    uint16_t id = ss_task_create(&info);
    if (id != 1 || ss_task_start(id) != SS_OK ||
        ss_task_sleep(3) != SS_OK || !worker_ran ||
        main_tcb.state != SS_TS_READY) {
        tty_puts("FAIL scheduler wakeup\n");
    } else {
        tty_puts("OK scheduler wakeup without scene\n");
    }
    for (;;) {}
    return 0;
}
