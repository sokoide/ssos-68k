#include "task_manager.h"

TaskControlBlock tcb_table[MAX_TASKS];
TaskControlBlock* ready_queue[MAX_TASK_PRI];
TaskControlBlock* curr_task;
TaskControlBlock* scheduled_task;
TaskControlBlock* wait_queue;
uint32_t dispatch_running = 0;
uint32_t global_counter = 0;

uint16_t main_task_id;
void initial_task_func(int16_t stacd, void* exinf);

// for some reason, if the function returns non 0,
// the caller (timerd_interrupt_handler) will crash
void timer_interrupt_handler() {
    global_counter++;
    if (global_counter % 16 == 0) {
        // switch context
    }
    // don't switch context
}