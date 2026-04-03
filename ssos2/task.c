/*
 * SSOS2 Cooperative Multitasking - Task Manager
 *
 * Round-robin scheduler with setjmp/longjmp-style context switching.
 * The assembly routine task_switch_to() saves/restores callee-saved
 * registers (d2-d7, a2-a6) on each task's stack.
 *
 * Stack layout for a new task:
 *   [bottom of stack area ... top]
 *                    sp+0..sp+43 : saved registers (d2-d7/a2-a6)
 *                   sp+44        : task entry point (rts target)
 *                   sp+48        : task_done (safety return)
 *
 * The main task (id 0) uses the C runtime stack — its SP is captured
 * on the first call to task_yield().
 */

#include "task.h"
#include <string.h>

/* --- Task table --- */
static Task tasks[MAX_TASKS];
static int num_tasks;
static int current_id;

/* Forward declaration: called if a task function accidentally returns */
void task_done(void);

/* ===========================================================
 * Initialization
 * =========================================================== */

void task_init(void) {
    memset(tasks, 0, sizeof(tasks));
    num_tasks = 1; /* task 0 = main */
    current_id = 0;
    tasks[0].active = 1;
    tasks[0].sp = 0; /* captured on first yield */
    tasks[0].entry = 0;
}

/* ===========================================================
 * Create a new task
 * =========================================================== */

int task_create(void (*func)(void)) {
    if (num_tasks >= MAX_TASKS)
        return 0;
    if (func == 0)
        return 0;

    int id = num_tasks;
    Task* t = &tasks[id];

    t->entry = func;
    t->active = 1;

    /* Build initial stack frame.
     * Stack grows downward from stack + TASK_STACK_SIZE. */
    uint32_t* sp = (uint32_t*)(t->stack + TASK_STACK_SIZE);

    /* Safety: if func() returns, jump to task_done */
    *--sp = (uint32_t)task_done;

    /* "Return address" for the rts in task_switch_to → enters func */
    *--sp = (uint32_t)func;

    /* Space for 11 callee-saved registers (d2-d7/a2-a6) = 44 bytes */
    sp -= 11;
    /* Register values don't matter for initial context */

    t->sp = (uint8_t*)sp;
    num_tasks++;
    return id;
}

/* ===========================================================
 * Yield: switch to next ready task
 * =========================================================== */

void task_yield(void) {
    if (num_tasks <= 1)
        return; /* nothing to switch to */

    /* Find next active task (round-robin) */
    int next = current_id;
    for (int i = 1; i < num_tasks; i++) {
        int candidate = (current_id + i) % num_tasks;
        if (tasks[candidate].active) {
            next = candidate;
            break;
        }
    }
    if (next == current_id)
        return; /* no other active task */

    int prev = current_id;
    current_id = next;
    task_switch_to(&tasks[prev].sp, tasks[next].sp);
}

/* ===========================================================
 * Task exit handler
 * =========================================================== */

void task_done(void) {
    tasks[current_id].active = 0;
    /* Yield to another task — this never returns since we're inactive */
    task_yield();
    /* Should never reach here */
    for (;;)
        ;
}

/* ===========================================================
 * Utility
 * =========================================================== */

int task_current_id(void) { return current_id; }
