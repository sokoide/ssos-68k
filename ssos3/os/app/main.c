#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include "../kernel/work_queue.h"
#include "../mem/memory.h"
#include "../gfx/gfx.h"
#include "../win/win.h"
#include "../ipc/ipc.h"

uint8_t* s3_task_stack_base;

void s3_init(void) {
    s3_mem_init((void*)&__ssosram_start, (uintptr_t)&__ssosram_size);
    s3_task_stack_base = (uint8_t*)s3_alloc(S3_MAX_TASKS * S3_TASK_STACK);

    s3_sched_init();
    s3_work_init(&s3_main_work_queue);
    s3_ipc_init();

    s3_gfx_init();
    s3_win_init();
}

void s3_run(void) {
    /* Create demo windows */
    uint16_t w1 = s3_win_create(50, 50, 300, 200, 1);
    uint16_t w2 = s3_win_create(200, 150, 300, 200, 2);
    (void)w1; (void)w2;

    /* Initial render */
    s3_win_render_all();

    /* Main loop */
    while (1) {
        s3_work_drain(&s3_main_work_queue);
        s3_process_wakeups();
    }
}
