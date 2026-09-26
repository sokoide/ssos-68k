#include "main_task.h"
#include "tty.h"

static SSTask main_tcb;
static int argument;
static volatile int entered;

static void* worker(void* arg) {
    if (arg != &argument) {
        tty_puts("FAIL entry argument\n");
        for (;;) {}
    }
    entered = 1;
    return arg;
}

int main(void) {
    ss_sched_init();
    if (ss_main_task_register(&main_tcb, 15) != SS_OK) {
        tty_puts("FAIL main register\n");
        for (;;) {}
    }
    SSTaskInfo info = { .entry = worker, .arg = &argument, .pri = 1 };
    uint16_t id = ss_task_create(&info);
    if (id != 1 || ss_task_start(id) != SS_OK) {
        tty_puts("FAIL task setup\n");
        for (;;) {}
    }
    ss_task_yield();
    if (!entered || tcb_table[0].state != SS_TS_TERMINATED ||
        ss_task_start(id) != (uint16_t)SS_ERR_STATE ||
        ready_queue.heads[1] != NULL) {
        tty_puts("FAIL task exit\n");
    } else {
        tty_puts("OK task argument + return\n");
    }
    for (;;) {}
    return 0;
}
