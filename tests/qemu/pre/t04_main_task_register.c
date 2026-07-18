/* t04_main_task_register.c - production main-task bootstrap round trip. */

#include "main_task.h"
#include "tty.h"

static SSTask main_tcb;
static volatile unsigned worker_ran;

static inline void emulate_timer_tick(void) {
    __asm__ volatile ("trap #0" ::: "memory");
}

static void fail(const char* reason) {
    tty_puts("FAIL ");
    tty_puts(reason);
    tty_puts("\n");
    for (;;) { }
}

static void* worker(void* arg) {
    (void)arg;
    worker_ran = 1;
    tty_putc('W');
    emulate_timer_tick();
    for (;;) {
        emulate_timer_tick();
    }
    return 0;
}

int main(void) {
    tty_puts("START main-register\n");
    ss_sched_init();

    if (ss_main_task_register(&main_tcb, 8) != SS_OK ||
        main_tcb.context != (void*)1 ||
        main_tcb.stack_base != (void*)1 ||
        main_tcb.entry != NULL ||
        main_tcb.state != SS_TS_READY ||
        main_tcb.pri != 8 ||
        ss_curr_task != &main_tcb) {
        fail("main bootstrap");
    }

    SSTaskInfo info = {
        .entry = worker, .pri = 8, .ctx_level = 0,
        .stack_size = SS_TASK_STACK, .stack = NULL,
    };
    uint16_t id = ss_task_create(&info);
    if (id == 0 || ss_task_start(id) != SS_OK) {
        fail("worker setup");
    }

    emulate_timer_tick();
    if (!worker_ran || ss_curr_task != &main_tcb ||
        main_tcb.context == (void*)1) {
        fail("main-worker-main");
    }

    tty_puts("M\nOK main-register\n");
    for (;;) { }
    return 0;
}
