/* t03_sleep_wakeup.c - verify ss_task_sleep actually blocks and wakes under
 * real preemption.
 *
 * task1 prints 'A', sleeps 3 ticks (-> WAIT, yields to main), and prints 'B'
 * when resumed. main fires `trap #0` repeatedly; each trap bumps ss_tick_counter
 * inside the ISR and reaps wakeups. Once enough ticks elapse, task1 is moved
 * back to READY and resumed via the rte path.
 *
 * Native tests can drive the scheduler's wait_until state machine by hand, but
 * they can't show a task *actually* yielding the CPU, idling while another
 * context runs, and resuming — only QEMU can.
 */

#include "scheduler.h"   /* kernel.h */
#include "tty.h"

static volatile int phase = 0;   /* 1 = task1 has gone to sleep, 2 = task1 woke */

static inline void emulate_timer_tick(void) {
    __asm__ volatile ("trap #0" ::: "memory");
}

static void* task1(void* arg) {
    (void)arg;
    tty_putc('A');
    phase = 1;                  /* about to sleep */
    ss_task_sleep(3);           /* WAIT + yield to main; resumed after 3 ticks */
    tty_putc('B');
    phase = 2;
    for (;;) emulate_timer_tick();
    return 0;
}

static SSTask main_tcb;

int main(void) {
    tty_puts("START sleep-wake\n");

    ss_sched_init();
    main_tcb.pri        = 8;
    main_tcb.stack_base = (void*)1;
    main_tcb.state      = SS_TS_READY;
    ss_curr_task        = &main_tcb;
    ss_sched_enqueue(&main_tcb);

    SSTaskInfo info = { .entry = task1, .pri = 8, .stack_size = SS_TASK_STACK, .stack = NULL };
    uint16_t id = ss_task_create(&info);
    ss_task_start(id);

    /* Pump timer ticks. The first trap runs task1 (it prints 'A' then sleeps);
     * subsequent traps advance ss_tick_counter until task1's wait_until is
     * reached, at which point the ISR reaps it and we land back in task1 ('B'). */
    int guard = 0;
    while (phase < 2 && guard++ < 50) {
        emulate_timer_tick();
    }

    if (phase == 2) {
        tty_puts("\nOK woke after sleep\n");
    } else {
        tty_puts("\nFAIL timeout\n");
    }
    for (;;) { }
    return 0;
}
