#include "status.h"
#include "clock.h"
#include "main_task.h"
#include "task_probe.h"
#include "tty.h"
#include "ipc.h"
extern volatile uint8_t ss_wakeups_needed;
static SSTask main_tcb;
static void pump(void) { ss_tick_counter++; ss_wakeups_needed = 1; }
int main(void) {
    ss_sched_init(); ss_ipc_init(); ss_main_task_register(&main_tcb, 8);
    if (ss_task_probe_run(pump) != SS_OK || ss_task_probe_result.completed != 32)
        tty_puts("FAIL shared target worker probe\n");
    else tty_puts("OK shared target worker probe 32 stacks/sleep/IPC/return\n");
    for (;;) {}
}
