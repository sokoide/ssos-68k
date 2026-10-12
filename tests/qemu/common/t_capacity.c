#include "status.h"
#include "clock.h"
#include "main_task.h"
#include "tty.h"

extern volatile uint8_t ss_wakeups_needed;
static SSTask main_tcb;
static volatile unsigned completed;
static uint32_t min_stack = SS_TASK_STACK, max_late;
static void fail(void) { tty_puts("FAIL capacity/sleep/stack\n"); for (;;) {} }
static void* worker(void* arg) {
    unsigned id = (unsigned)(uintptr_t)arg;
    static const unsigned waits[] = {1, 5, 10};
    for (unsigned i = 0; i < 3; i++) {
        uint32_t start = ss_tick_counter;
        if (ss_task_sleep(waits[i]) != SS_OK || ss_tick_counter - start < waits[i]) fail();
        uint32_t remaining = ss_stack_check((uint16_t)id);
        if (remaining == 0) fail();
        if (remaining < min_stack) min_stack = remaining;
        uint32_t late = ss_tick_counter - start - waits[i];
        if (late > max_late) max_late = late;
    }
    completed++;
    return NULL;
}
int main(void) {
    static const unsigned counts[] = {1, 2, 8, 32};
    for (unsigned phase = 0; phase < 4; phase++) {
        ss_sched_init(); completed = 0;
        if (ss_main_task_register(&main_tcb, 15) != SS_OK) fail();
        for (unsigned i = 0; i < counts[phase]; i++) {
            SSTaskInfo info = {.entry = worker, .pri = 1, .arg = (void*)(uintptr_t)(i + 1)};
            uint16_t id = ss_task_create(&info);
            if (id != i + 1 || ss_task_start(id) != SS_OK) fail();
        }
        for (unsigned guard = 0; completed < counts[phase] && guard < 1000; guard++) {
            ss_tick_counter++;
            ss_wakeups_needed = 1;
            ss_task_yield();
        }
        if (completed != counts[phase]) fail();
        for (unsigned i = 0; i < counts[phase]; i++)
            if (tcb_table[i].state != SS_TS_TERMINATED || ss_stack_check(i + 1) == 0) fail();
    }
    tty_puts("PROBE min_stack_free_bytes="); tty_putu(min_stack);
    tty_puts(" max_late_synthetic_ticks="); tty_putu(max_late); tty_puts("\n");
    tty_puts("OK capacity 1/2/8/32 sleep 1/5/10\n");
    for (;;) {}
}
