#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include "../kernel/work_queue.h"
#include "../mem/memory.h"
#include "../gfx/gfx.h"
#include "../win/win.h"
#include "../ipc/ipc.h"
#include "scene.h"

#include <stdint.h>

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
    ss_scene_run(NULL, NULL);
}
