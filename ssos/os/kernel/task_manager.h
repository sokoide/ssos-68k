#pragma once

#include "kernel.h"
#include "task_manager.h"

// global variables
extern TaskControlBlock tcb_table[];
extern TaskControlBlock* ready_queue[];
extern TaskControlBlock* curr_task;
extern TaskControlBlock*
    scheduled_task; // scheduled task which will be executed after the curr_task
extern TaskControlBlock* wait_queue;
extern uint32_t dispatch_running;
extern FlagControlBlock fcb_table[];
extern uint32_t global_counter;

// global functions
int timer_interrupt_handler();
