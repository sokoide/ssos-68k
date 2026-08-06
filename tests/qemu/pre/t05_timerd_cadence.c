/* Verify the production Timer D switch cadence (10 hardware ticks).
 *
 * QEMU has no MFP Timer D, so trap #0 synchronously enters the same ported
 * handler.  Nine ticks must return straight to main without changing the
 * current task.  Tick 10 context-switches main through the interrupted/rte
 * path, starts the worker, and returns to main after the worker sleeps.
 *
 * The worker's deadline is tick 15.  Wakeups run only on a switch tick in the
 * production handler, therefore it remains WAIT through tick 19 and resumes
 * at tick 20.  It voluntarily yields immediately so main again returns from
 * the interrupted context via rte and can make the assertions below.
 */

#include "scheduler.h"
#include "tty.h"

extern volatile uint32_t ss_tick_counter;
extern uint32_t ss_context_switch_count;

static volatile unsigned phase;
static SSTask main_tcb;

static inline void emulate_timer_tick(void) {
    __asm__ volatile ("trap #0" ::: "memory");
}

static int check(int condition, const char* message) {
    if (condition)
        return 1;
    tty_puts("FAIL ");
    tty_puts(message);
    tty_puts("\n");
    return 0;
}

static void* sleeper(void* arg) {
    (void)arg;
    phase = 1;
    ss_task_sleep(5);             /* tick 10 + 5; reaped at switch tick 20 */
    phase = 2;
    ss_task_yield();              /* let interrupted main resume by rte */
    for (;;) emulate_timer_tick();
    return 0;
}

int main(void) {
    int ok = 1;
    tty_puts("START pre cadence10\n");

    ss_sched_init();
    main_tcb.pri = 8;
    main_tcb.stack_base = (void*)1;
    main_tcb.state = SS_TS_READY;
    ss_curr_task = &main_tcb;
    ss_sched_enqueue(&main_tcb);

    SSTaskInfo info = {
        .entry = sleeper, .pri = 8, .ctx_level = 0,
        .stack_size = SS_TASK_STACK, .stack = NULL,
    };
    uint16_t id = ss_task_create(&info);
    ok &= check(id == 1, "task creation");
    ok &= check(ss_task_start(id) == SS_OK, "task start");

    for (unsigned i = 0; i < 9; i++)
        emulate_timer_tick();
    ok &= check(ss_tick_counter == 9, "nine ticks counted");
    ok &= check(ss_context_switch_count == 0, "switched before tick 10");
    ok &= check(phase == 0, "worker ran before tick 10");
    ok &= check(ss_curr_task == &main_tcb, "current changed before tick 10");

    emulate_timer_tick();
    ok &= check(ss_tick_counter == 10, "tenth tick counted");
    ok &= check(ss_context_switch_count == 1, "no switch at tick 10");
    ok &= check(phase == 1, "worker did not sleep at tick 10");
    ok &= check(tcb_table[0].state == SS_TS_WAIT, "worker not waiting");
    ok &= check(ss_curr_task == &main_tcb, "main did not return through rte");

    for (unsigned i = 0; i < 9; i++)
        emulate_timer_tick();
    ok &= check(ss_tick_counter == 19, "nineteenth tick counted");
    ok &= check(ss_context_switch_count == 1, "switched before tick 20");
    ok &= check(phase == 1, "worker woke before switch cadence");
    ok &= check(tcb_table[0].state == SS_TS_WAIT, "worker left wait early");

    emulate_timer_tick();
    ok &= check(ss_tick_counter == 20, "twentieth tick counted");
    ok &= check(ss_context_switch_count == 2, "no switch at tick 20");
    ok &= check(phase == 2, "worker did not wake at tick 20");
    ok &= check(tcb_table[0].state == SS_TS_READY, "worker not ready after wake");
    ok &= check(ss_curr_task == &main_tcb, "main did not return after wake");

    tty_puts(ok ? "OK cadence10 + sleep wake cadence\n" : "FAIL cadence10\n");
    for (;;) { }
    return 0;
}
