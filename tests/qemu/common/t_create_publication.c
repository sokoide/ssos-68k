#include "main_task.h"
#include "tty.h"

static SSTask main_tcb;
static volatile int entered, probed;
static void* worker(void* arg) {
    if (arg != (void*)42) { tty_puts("FAIL argument\n"); for (;;) {} }
    entered++;
    return arg;
}
void ss_task_create_probe(uint16_t id) {
    if (tcb_table[id - 1].state != SS_TS_INITIALIZING ||
        ss_task_start(id) != (uint16_t)SS_ERR_STATE || entered) {
        tty_puts("FAIL initialization publication\n"); for (;;) {}
    }
    ss_task_yield();
    if (entered) { tty_puts("FAIL premature entry\n"); for (;;) {} }
    probed++;
}
int main(void) {
    ss_sched_init();
    ss_main_task_register(&main_tcb, 15);
    SSTaskInfo info = {.entry = worker, .arg = (void*)42, .pri = 1};
    uint16_t id = ss_task_create(&info);
    if (probed != 1 || ss_task_start(id) != SS_OK) {
        tty_puts("FAIL start\n"); for (;;) {}
    }
    ss_task_yield();
    if (entered != 1 || tcb_table[id - 1].state != SS_TS_TERMINATED ||
        ss_stack_check(id) == 0) tty_puts("FAIL completed task\n");
    else tty_puts("OK initialization publication + stack\n");
    for (;;) {}
}
