#pragma once

#include "kernel.h"
#include "task_manager.h"

// task attributes
#define TA_HLNG 0x0000001    // the task is written in high level language
#define TA_USERBUF 0x0000020 // stack to use a buffer specified by a user
#define TA_RNG0 0x0000000    // the task to run in ring 0
#define TA_RNG1 0x0000100    // the task to run in ring 1
#define TA_RNG2 0x0000200    // the task to run in ring 2
#define TA_RNG3 0x0000300    // the task to run in ring 3
// task wait attributes
#define TA_TFIFO 0x00000000 // maintain wait tasks in FIFO order
#define TA_TPRI 0x00000001  // maintain wait tasks in priority order
#define TA_FIRST 0x00000000 // prioritize the first task in the queue
#define TA_CNT 0x00000002   // prioritize a task which has less requests
#define TA_WSGL 0x00000000  // don't allow multiple task wait
#define TA_WMUL 0x00000008  // allow multiple task wait


// structs
typedef struct {
    void* exinf; // extended info, not used
    uint16_t task_attr;
    FUNCPTR task;
    int8_t task_pri;
    int32_t stack_size;
    void* stack;
} TaskInfo;

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
extern uint16_t main_task_id;
extern TaskInfo main_task;

// global functions
int timer_interrupt_handler();
uint16_t ss_create_task(const TaskInfo* ti);
uint16_t ss_start_task(uint16_t id, int16_t stacd /* not used */);
