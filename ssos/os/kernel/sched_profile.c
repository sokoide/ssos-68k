#include "irq.h"
#include "clock.h"
#include "sched_profile.h"
#include "scheduler.h"
#include <string.h>
#if SS_PROFILE_RUNTIME
#if SS_PROFILE_RUNTIME
static SSSchedProfile profile;
static uint32_t ready_tick[SS_MAX_TASKS];
static uint8_t valid[SS_MAX_TASKS];
static int index_of(SSTask* tcb) {
    for (int i = 0; i < SS_MAX_TASKS; i++)
        if (tcb == &tcb_table[i]) return i;
    return -1;
}
#endif
void ss_sched_profile_reset(void) {
#if SS_PROFILE_RUNTIME
    memset(&profile, 0, sizeof(profile));
    memset(valid, 0, sizeof(valid));
#endif
}
void ss_sched_profile_ready(SSTask* task) {
#if SS_PROFILE_RUNTIME
    int i = index_of(task);
    if (i >= 0) { ready_tick[i] = ss_tick_counter; valid[i] = 1; }
#else
    (void)task;
#endif
}
void ss_sched_profile_run(SSTask* task) {
#if SS_PROFILE_RUNTIME
    int i = index_of(task);
    profile.switches++;
    if (i >= 0 && valid[i]) {
        uint32_t delay = ss_tick_counter - ready_tick[i];
        if (delay > profile.max_ready_to_run_ticks) profile.max_ready_to_run_ticks = delay;
        valid[i] = 0;
    }
#else
    (void)task;
#endif
}
void ss_sched_profile_wake(uint32_t lateness) {
#if SS_PROFILE_RUNTIME
    profile.wakeups++;
    if (lateness > profile.max_deadline_to_ready_ticks) profile.max_deadline_to_ready_ticks = lateness;
#else
    (void)lateness;
#endif
}
void ss_sched_profile_snapshot(SSSchedProfile* out) {
    if (out == NULL) return;
#if SS_PROFILE_RUNTIME
    uint16_t sr = ss_irq_save();
    *out = profile;
    ss_irq_restore(sr);
#else
    memset(out, 0, sizeof(*out));
#endif
}

#endif
