#ifndef SS_SCHED_PROFILE_H
#define SS_SCHED_PROFILE_H
#include <stdint.h>
#ifndef SS_PROFILE_RUNTIME
#define SS_PROFILE_RUNTIME 0
#endif
struct SSTask;
typedef struct {
    uint32_t switches, wakeups, max_deadline_to_ready_ticks, max_ready_to_run_ticks;
} SSSchedProfile;
/* Optional coarse tick telemetry. It never measures time with IRQs masked. */
#if SS_PROFILE_RUNTIME
void ss_sched_profile_reset(void);
void ss_sched_profile_ready(struct SSTask* task);
void ss_sched_profile_run(struct SSTask* task);
void ss_sched_profile_wake(uint32_t lateness);
void ss_sched_profile_snapshot(SSSchedProfile* out);
#else
static inline void ss_sched_profile_reset(void) {}
static inline void ss_sched_profile_ready(struct SSTask* task) { (void)task; }
static inline void ss_sched_profile_run(struct SSTask* task) { (void)task; }
static inline void ss_sched_profile_wake(uint32_t lateness) { (void)lateness; }
static inline void ss_sched_profile_snapshot(SSSchedProfile* out) {
    if (out != 0) *out = (SSSchedProfile){0};
}
#endif
#endif
