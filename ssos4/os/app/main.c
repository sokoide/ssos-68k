#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include "../kernel/work_queue.h"
#include "../mem/memory.h"
#include "../gfx/gfx.h"
#include "../win/win.h"
#include "../ipc/ipc.h"

uint8_t* s4_task_stack_base;

void s4_init(void) {
    s4_mem_init((void*)&__ssosram_start, (uintptr_t)&__ssosram_size);
    s4_task_stack_base = (uint8_t*)s4_alloc(S4_MAX_TASKS * S4_TASK_STACK);

    s4_sched_init();
    s4_work_init(&s4_main_work_queue);
    s4_ipc_init();

    s4_gfx_init();
    s4_win_init();
}

void s4_run(void) {
    /* Create demo windows */
    uint16_t w1 = s4_win_create(50, 50, 300, 200, 1);
    uint16_t w2 = s4_win_create(200, 150, 300, 200, 2);
    (void)w1; (void)w2;

    /* Initial render */
    s4_win_render_all();

    /* Main loop */
    while (1) {
        s4_work_drain(&s4_main_work_queue);
        s4_process_wakeups();
    }
}
