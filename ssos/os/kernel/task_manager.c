#include "task_manager.h"
#include "errors.h"
#include "memory.h"
#include "ssosmain.h"
#include <stdio.h>

TaskControlBlock tcb_table[MAX_TASKS];
TaskControlBlock* ready_queue[MAX_TASK_PRI];
TaskControlBlock* curr_task;
TaskControlBlock* scheduled_task;
TaskControlBlock* wait_queue;
uint32_t dispatch_running = 0;
uint32_t global_counter = 0;

uint16_t main_task_id;
void initial_task_func(int16_t stacd, void* exinf);

TaskInfo main_task = {
    .task_attr = TA_HLNG | TA_RNG0 | TA_USERBUF,
    .task = initial_task_func,
    .task_pri = 1,
    .stack_size = TASK_STACK_SIZE,
    .stack = NULL,
};

void initial_task_func(int16_t stacd /* not used */,
                       void* exinf /* not used */) {
    ssosmain();
    /* ss_exit_task(); */
}
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

uint16_t ss_create_task(const TaskInfo* ti) {
    uint16_t id;
    uint16_t i;

    if (ti->task_attr & ~(TA_RNG3 | TA_HLNG | TA_USERBUF))
        return E_RSATR;
    if (ti->task_pri <= 0 || ti->task_pri > MAX_TASK_PRI)
        return E_PAR;
    if ((ti->task_attr & TA_USERBUF) && (ti->stack_size == 0))
        return E_PAR;

    disable_interrupts();

    for (i = 0; i < MAX_TASKS; i++) {
        // find unused TCB
        if (tcb_table[i].state == TS_NONEXIST)
            break;
    }

    // init tcb_table
    if (i < MAX_TASKS) {
        tcb_table[i].state = TS_DORMANT;
        tcb_table[i].prev = NULL;
        tcb_table[i].next = NULL;

        tcb_table[i].task_addr = ti->task;
        tcb_table[i].task_pri = ti->task_pri;
        if (ti->task_attr & TA_USERBUF) {
            tcb_table[i].stack_size = ti->stack_size;
            tcb_table[i].stack_addr = ti->stack;
        } else {
            tcb_table[i].stack_size = TASK_STACK_SIZE;
            tcb_table[i].stack_addr =
                ss_task_stack_base + (i + 1) * TASK_STACK_SIZE - 1;
        }

        id = i + 1;
    } else {
        id = (uint16_t)E_LIMIT;
    }

    enable_interrupts();
    return id;
}

uint16_t ss_start_task(uint16_t id, int16_t stacd /* not used */) {
    TaskControlBlock* tcb;
    uint16_t err = E_OK;

    if (id <= 0 || id > MAX_TASKS)
        return E_ID;
    disable_interrupts();

    /* tcb = &tcb_table[id - 1]; */
    /* if (tcb->state == TS_DORMANT) { */
    /*     tcb->state = TS_READY; */
    /*     tcb->context = */
    /*         make_context(tcb->stack_addr, tcb->stack_size, tcb->task_addr);
     */
    /*     task_queue_add_entry(&ready_queue[tcb->task_pri], tcb); */
    /*     scheduler(); */
    /* } else { */
    /*     err = E_OBJ; */
    /* } */

    enable_interrupts();
    return err;
}
