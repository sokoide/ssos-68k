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

__attribute__((optimize("no-stack-protector", "omit-frame-pointer"))) int
timer_interrupt_handler() {
    global_counter++;
    if (global_counter % 16 == 0) {
        // switch context
        return 1;
    }
    // don't switch context
    return 0;
}
