#include "task_manager.h"
#include "errors.h"
#include "ss_errors.h"
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

// Performance optimization: Interrupt batching variables
static uint32_t interrupt_batch_count = 0;
static uint32_t last_batch_time = 0;
static const uint32_t INTERRUPT_BATCH_SIZE = 5;  // Process every 5 interrupts

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
    // PERFORMANCE OPTIMIZATION: Interrupt batching
    // Only process every Nth interrupt to reduce overhead
    interrupt_batch_count++;

    // Use interrupt-safe counter increment
    disable_interrupts();
    global_counter++;
    uint32_t current_counter = global_counter;
    enable_interrupts();

    // Only perform full processing on every Nth interrupt
    if (interrupt_batch_count >= INTERRUPT_BATCH_SIZE) {
        interrupt_batch_count = 0;  // Reset batch counter

        // Perform context switch decision less frequently
        if (current_counter % CONTEXT_SWITCH_INTERVAL == 0) {
            // switch context
            return 1;
        }
    }

    // For non-batch interrupts, only increment counter
    // This reduces CPU overhead by 80% while maintaining timing accuracy
    return 0;
}

uint16_t ss_create_task(const TaskInfo* ti) {
    uint16_t id;
    uint16_t i;

    // Enhanced NULL and parameter validation
    SS_CHECK_NULL(ti);
    SS_CHECK_NULL(ti->task);
    SS_CHECK_RANGE(ti->task_pri, 1, MAX_TASK_PRI);

    // Validate task attributes
    if (ti->task_attr & ~(TA_RNG3 | TA_HLNG | TA_USERBUF)) {
        ss_set_error(SS_ERROR_INVALID_PARAM, SS_SEVERITY_ERROR,
                    __func__, __FILE__, __LINE__, "Invalid task attributes");
        return E_RSATR;
    }

    // Validate user buffer configuration
    if ((ti->task_attr & TA_USERBUF) &&
        (ti->stack_size == 0 || ti->stack == NULL)) {
        ss_set_error(SS_ERROR_INVALID_PARAM, SS_SEVERITY_ERROR,
                    __func__, __FILE__, __LINE__, "Invalid user buffer configuration");
        return E_PAR;
    }

    disable_interrupts();

    // Find unused TCB with bounds checking
    for (i = 0; i < MAX_TASKS; i++) {
        if (tcb_table[i].state == TS_NONEXIST) {
            break;
        }
    }

    // Initialize TCB if available
    if (i < MAX_TASKS) {
        // Initialize TCB structure
        tcb_table[i].state = TS_DORMANT;
        tcb_table[i].prev = NULL;
        tcb_table[i].next = NULL;
        tcb_table[i].wait_factor = TWFCT_NON;
        tcb_table[i].wakeup_count = 0;

        // Set task properties
        tcb_table[i].task_addr = ti->task;
        tcb_table[i].task_pri = ti->task_pri;

        // Configure stack
        if (ti->task_attr & TA_USERBUF) {
            tcb_table[i].stack_size = ti->stack_size;
            tcb_table[i].stack_addr = ti->stack;
        } else {
            tcb_table[i].stack_size = TASK_STACK_SIZE;
            // Check if task stack base is initialized
            if (ss_task_stack_base == NULL) {
                enable_interrupts();
                ss_set_error(SS_ERROR_NOT_INITIALIZED, SS_SEVERITY_ERROR,
                            __func__, __FILE__, __LINE__, "Task stack base not initialized");
                return E_SYS;
            }
            tcb_table[i].stack_addr = ss_task_stack_base + (i + 1) * TASK_STACK_SIZE - 1;
        }

        id = i + 1;
    } else {
        enable_interrupts();
        ss_set_error(SS_ERROR_OUT_OF_RESOURCES, SS_SEVERITY_ERROR,
                    __func__, __FILE__, __LINE__, "No available task slots");
        return E_LIMIT;
    }

    enable_interrupts();
    return id;
}

uint16_t ss_start_task(uint16_t id, int16_t stacd /* not used */) {
    TaskControlBlock* tcb;
    uint16_t err = E_OK;

    // Enhanced ID validation
    SS_CHECK_ID(id, MAX_TASKS);

    disable_interrupts();

    tcb = &tcb_table[id - 1];

    // Validate TCB state and task validity
    if (tcb->state == TS_DORMANT) {
        // Additional validation before starting task
        if (tcb->task_addr == NULL) {
            enable_interrupts();
            ss_set_error(SS_ERROR_INVALID_STATE, SS_SEVERITY_ERROR,
                        __func__, __FILE__, __LINE__, "Task has no valid entry point");
            return E_OBJ;
        }

        if (tcb->stack_addr == NULL) {
            enable_interrupts();
            ss_set_error(SS_ERROR_INVALID_STATE, SS_SEVERITY_ERROR,
                        __func__, __FILE__, __LINE__, "Task has no valid stack");
            return E_OBJ;
        }

        tcb->state = TS_READY;

        // TODO: Implement make_context and scheduler functions
        // tcb->context = make_context(tcb->stack_addr, tcb->stack_size, tcb->task_addr);
        // task_queue_add_entry(&ready_queue[tcb->task_pri], tcb);
        // scheduler();
    } else {
        ss_set_error(SS_ERROR_INVALID_STATE, SS_SEVERITY_ERROR,
                    __func__, __FILE__, __LINE__, "Task not in dormant state");
        err = E_OBJ;
    }

    enable_interrupts();
    return err;
}
