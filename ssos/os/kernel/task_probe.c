#include "task_probe.h"
#include "scheduler.h"
#if SS_DIAG_TASKS && !defined(LOCAL_MODE)
#include "../ipc/ipc.h"
#endif
volatile SSTaskProbeResult ss_task_probe_result;
#if SS_DIAG_TASKS
static uint16_t probe_ids[SS_DIAG_TASKS];
static void* probe_worker(void* arg) {
    uint16_t id = *(uint16_t*)arg;
#if !defined(LOCAL_MODE)
    unsigned index = (uint16_t*)arg - probe_ids;
#endif
    static const uint16_t waits[] = {1, 5, 10};
    for (unsigned i = 0; i < 3; i++) {
        uint32_t start = ss_tick_counter;
        if (ss_task_sleep(waits[i]) != SS_OK) {
            ss_task_probe_result.status = SS_ERR_STATE;
            return NULL;
        }
        uint32_t elapsed = ss_tick_counter - start;
        uint32_t remaining = ss_stack_check(id);
        uint16_t sr = ss_irq_save();
        if (elapsed < waits[i] || !remaining) ss_task_probe_result.status = SS_ERR_STATE;
        else {
            if (elapsed - waits[i] > ss_task_probe_result.max_late_ticks)
                ss_task_probe_result.max_late_ticks = elapsed - waits[i];
            if (remaining < ss_task_probe_result.min_stack_free)
                ss_task_probe_result.min_stack_free = remaining;
        }
        ss_irq_restore(sr);
    }
#if !defined(LOCAL_MODE)
    SSMessage msg = {.type = 1, .sender = id, .payload_size = 1};
    msg.payload[0] = (uint8_t)index;
    uint16_t next = probe_ids[(index + 1) % SS_DIAG_TASKS];
    unsigned previous = (index + SS_DIAG_TASKS - 1) % SS_DIAG_TASKS;
    if (ss_send(next, &msg) != SS_OK || ss_recv(&msg) != SS_OK ||
        msg.sender != probe_ids[previous] || msg.payload[0] != previous) {
        ss_task_probe_result.status = SS_ERR_STATE;
        return NULL;
    }
#endif
    uint16_t sr = ss_irq_save();
    ss_task_probe_result.completed++;
    ss_irq_restore(sr);
    return arg;
}
#endif
int16_t ss_task_probe_run(void (*pump)(void)) {
#if SS_DIAG_TASKS
    ss_task_probe_result = (SSTaskProbeResult){0, SS_TASK_STACK, 0, SS_OK};
    if (ss_curr_task == NULL) return SS_ERR_STATE;
    for (unsigned i = 0; i < SS_DIAG_TASKS; i++) {
        SSTaskInfo info = {.entry = probe_worker, .pri = ss_curr_task->pri, .arg = &probe_ids[i]};
        uint16_t id = ss_task_create(&info);
        if (id == 0 || id > SS_MAX_TASKS) return SS_ERR_STATE;
        probe_ids[i] = id;
    }
    for (unsigned i = 0; i < SS_DIAG_TASKS; i++)
        if (ss_task_start(probe_ids[i]) != SS_OK) return SS_ERR_STATE;
    unsigned guard = 0;
    int finished = 0;
    while (ss_task_probe_result.status == SS_OK && guard++ < 1000000) {
        uint16_t sr = ss_irq_save();
        finished = ss_task_probe_result.completed == SS_DIAG_TASKS;
        for (unsigned i = 0; i < SS_DIAG_TASKS; i++)
            if (tcb_table[probe_ids[i] - 1].state != SS_TS_TERMINATED) finished = 0;
        ss_irq_restore(sr);
        if (finished) break;
        if (pump != NULL) pump();
        ss_task_yield();
    }
    if (!finished) return SS_ERR_STATE;
    for (unsigned i = 0; i < SS_DIAG_TASKS; i++)
        if (!ss_stack_check(probe_ids[i])) return SS_ERR_STATE;
    return ss_task_probe_result.status;
#else
    (void)pump;
    return SS_ERR_STATE;
#endif
}
