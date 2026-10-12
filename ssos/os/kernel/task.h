#ifndef SS_TASK_H
#define SS_TASK_H

#include <stdint.h>
#include "status.h"

/* Task constants */
#define SS_MAX_TASKS   32
#define SS_MAX_PRI     16
#define SS_TASK_STACK  16384
#define SS_MIN_TASK_STACK 256

/* Context save levels */
#define SS_CTX_MINIMAL 0x00
#define SS_CTX_NORMAL  0x01
#define SS_CTX_FULL    0x02

#define SS_MAX_SLEEP_TICKS 0x7FFFFFFFUL

typedef struct {
    void* (*entry)(void*); /* entry(arg); return value is ignored */
    uint8_t pri;
    uint8_t ctx_level;
    uint16_t stack_size;   /* custom stack must be >= SS_MIN_TASK_STACK */
    void*    stack;        /* NULL = pool; otherwise last aligned word of region */
    void*    arg;          /* passed to entry; NULL by default */
} SSTaskInfo;

uint16_t ss_task_create(SSTaskInfo* info);
uint16_t ss_task_start(uint16_t id);
uint16_t ss_task_sleep(uint32_t ticks);
void     ss_task_yield(void);
void     ss_task_exit(void) __attribute__((noreturn));
uint32_t ss_stack_check(uint16_t id);

/* Custom stack is a descending region [stack - stack_size + 4, stack + 4).
 * It remains caller-owned until system shutdown; it is not reclaimed.
 * Returning from entry calls ss_task_exit. The task becomes TERMINATED and
 * its slot/stack remain reserved; there is no join or stack reclamation yet.
 * ss_task_create returns IDs 1..SS_MAX_TASKS or a uint16_t-encoded SS_ERR_*.
 * ctx_level is accepted but all registers are currently saved at every switch.
 */

#endif /* SS_TASK_H */
