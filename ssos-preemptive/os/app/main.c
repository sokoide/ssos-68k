#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include "../kernel/work_queue.h"
#include "../mem/memory.h"
#include "../gfx/gfx.h"
#include "../win/win.h"
#include "../ipc/ipc.h"

uint8_t* ss_task_stack_base;

void ss_init(void) {
    ss_mem_init((void*)&__ssosram_start, (uintptr_t)&__ssosram_size);
    ss_task_stack_base = (uint8_t*)ss_alloc(SS_MAX_TASKS * SS_TASK_STACK);

    ss_sched_init();
    ss_work_init(&ss_main_work_queue);
    ss_ipc_init();

    ss_gfx_init();
    ss_win_init();
}

void ss_run(void) {
    /* Create demo windows */
    uint16_t w1 = ss_win_create(50, 50, 300, 200, 1);
    uint16_t w2 = ss_win_create(200, 150, 300, 200, 2);
    (void)w1; (void)w2;

    /* Initial render */
    ss_win_render_all();

    /* Main loop */
    while (1) {
        ss_work_drain(&ss_main_work_queue);
        ss_process_wakeups();
    }
}
