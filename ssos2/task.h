#pragma once
#include <stdint.h>

/* ============================================================
 * SSOS2 Cooperative Multitasking
 *
 * Lightweight round-robin scheduler for the 68000 CPU.
 * Each task has its own stack and yields explicitly.
 * Context switch saves/restores d2-d7/a2-a6 (callee-saved).
 *
 * Usage:
 *   1. task_init()           - initialize system
 *   2. task_create(...)      - create worker tasks
 *   3. Run main loop with task_yield() at each V-sync
 * ============================================================ */

#define MAX_TASKS 4
#define TASK_STACK_SIZE 4096

typedef struct {
    uint8_t* sp;                    /* saved stack pointer */
    uint8_t stack[TASK_STACK_SIZE]; /* task's private stack */
    void (*entry)(void);            /* entry function */
    int active;                     /* 1 = task is alive */
} Task;

/* Assembly context switch (in timer.s)
 *   void task_switch_to(uint8_t** current_sp_ptr, uint8_t* next_sp);
 * Saves callee-saved registers, switches SP, restores from new stack. */
extern void task_switch_to(uint8_t** current_sp, uint8_t* next_sp);

/* Initialize task system. Call once before creating tasks. */
void task_init(void);

/* Create a new task. Returns task ID (1-based), 0 on error. */
int task_create(void (*func)(void));

/* Yield to the next ready task. Returns when this task is scheduled again. */
void task_yield(void);

/* Get current task ID (0 = main task). */
int task_current_id(void);

/* Mark a task as dead (called when task function returns). */
void task_done(void);
