/* t_sleep_sr.c - ss_task_sleep must restore the caller's interrupt mask.
 *
 * scheduler.c's contract: ss_task_sleep() masks, sleeps, and restores the
 * saved SR after the wake yield returns. A task that sleeps at IPL N must
 * therefore still run at IPL N afterwards — not at the scheduler's internal
 * mask, and not at whatever IPL the waker or ISR held.
 *
 * cooperative variant (-DSS_COOP_TEST): main advances ss_tick_counter and
 * raises the wakeup flag, then yields so the switch boundary reaps the
 * sleeper. preemptive variant: main pumps trap #0; the port ISR bumps the
 * tick and reaps wakeups itself.
 */

#include "scheduler.h"
#include "main_task.h"
#include "tty.h"

static SSTask main_tcb;
static volatile int done;
static volatile int failed;
static volatile const char* fail_reason;

static void fail(const char* reason) {
    fail_reason = reason;
    failed = 1;
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

static const uint16_t test_ipls[] = {0, 3, 5, 7};

static void* sleeper(void* arg) {
    (void)arg;
    for (unsigned i = 0; i < sizeof(test_ipls) / sizeof(test_ipls[0]); i++) {
        write_ipl(test_ipls[i]);
        if (ss_task_sleep(2) != SS_OK) {
            fail("sleep");
            break;
        }
        uint16_t woke_ipl = (read_sr() & 0x0700u) >> 8;
        if (woke_ipl != test_ipls[i]) {
            fail("ipl not restored after sleep");
            break;
        }
    }
    write_ipl(0);
    done = 1;
    /* Returning (not endless yielding) matters: a yielded task stays READY,
     * so a lower-priority main would never be scheduled to print the
     * verdict. return -> ss_task_exit -> TERMINATED dequeues this task. */
    return 0;
}

#ifdef SS_COOP_TEST
extern volatile uint8_t ss_wakeups_needed;

static void pump_ticks(void) {
    ss_tick_counter += 1;
    ss_wakeups_needed = 1;
    ss_task_yield();
}
#else
static inline void pump_ticks(void) {
    __asm__ volatile ("trap #0" ::: "memory");
}
#endif

int main(void) {
    ss_sched_init();
    if (ss_main_task_register(&main_tcb, 10) != SS_OK) {
        tty_puts("FAIL register\n");
        for (;;) {}
    }
    SSTaskInfo info = { .entry = sleeper, .pri = 5 };
    uint16_t id = ss_task_create(&info);
    if (id != 1 || ss_task_start(id) != SS_OK) {
        tty_puts("FAIL task setup\n");
        for (;;) {}
    }

    int guard = 0;
    while (!done && !failed && guard++ < 400) {
        pump_ticks();
    }
    if (done && !failed) {
        tty_puts("OK sleep restores caller SR\n");
    } else {
        tty_puts("FAIL ");
        tty_puts(failed ? (const char*)fail_reason : "sleeper never woke");
        tty_puts("\n");
    }
    for (;;) {}
    return 0;
}
