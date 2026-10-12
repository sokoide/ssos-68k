#ifndef SS_TASK_PROBE_H
#define SS_TASK_PROBE_H
#include <stdint.h>
#ifndef SS_DIAG_TASKS
#define SS_DIAG_TASKS 0
#endif
_Static_assert(SS_DIAG_TASKS == 0 || SS_DIAG_TASKS == 1 || SS_DIAG_TASKS == 2 ||
               SS_DIAG_TASKS == 8 || SS_DIAG_TASKS == 32, "diagnostic task count");
typedef struct {
    uint32_t completed, min_stack_free, max_late_ticks;
    int16_t status;
} SSTaskProbeResult;
extern volatile SSTaskProbeResult ss_task_probe_result;
/* Opt-in boot diagnostic consumes worker slots; use a fresh process/image.
 * pump is NULL on X68000 (real Timer D), synthetic only in QEMU tests. */
int16_t ss_task_probe_run(void (*pump)(void));
#endif
