#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include "../kernel/work_queue.h"
#include "../mem/memory.h"
#include "../gfx/gfx.h"
#include "../win/win.h"
#include "../ipc/ipc.h"
#include "scene.h"
#include "../kernel/task_probe.h"

#include <stdint.h>

uint8_t* ss_task_stack_base;

int16_t ss_init(void) {
    ss_mem_init((void*)&__ssosram_start, (uintptr_t)&__ssosram_size);
    /* ss_mem_init bails out silently on a bad arena; do not hand a
     * half-initialized allocator to the rest of the boot. */
    if (!ss_mem_healthy())
        return SS_ERR_MEMORY;
    ss_task_stack_base = &__ssosstack_start;
    ss_sched_init();
    ss_work_init(&ss_main_work_queue);
    ss_ipc_init();
    ss_gfx_init();
    ss_win_init();
    return SS_OK;
}

void ss_run(void) {
#if SS_DIAG_TASKS
    if (ss_task_probe_run(NULL) != SS_OK) {
        (void)ss_irq_save();
        for (;;) {}
    }
#endif
    ss_scene_run(NULL, NULL);
    /* Baremetal cannot return DMA/display ownership to Human68K. */
    (void)ss_irq_save();
    for (;;) {}
}
