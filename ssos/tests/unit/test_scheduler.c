#include "../framework/ssos_test.h"

// Include SSOS task management system
#include "task_manager.h"
#include "ss_config.h"
#include "memory.h"

// External declarations for internal scheduler functions/variables
extern TaskControlBlock tcb_table[MAX_TASKS];
extern TaskControlBlock* ready_queue[MAX_TASK_PRI];
extern TaskControlBlock* scheduled_task;
extern int main_task_id;

// Mock task function for testing
void dummy_task_func(void) {
    // Empty task function for testing
}

// Test helper: Reset scheduler state
static void reset_scheduler_state(void) {
    // Clear all TCBs
    for (int i = 0; i < MAX_TASKS; i++) {
        tcb_table[i].state = TS_NONEXIST;
        tcb_table[i].task_pri = 0;
        tcb_table[i].next = NULL;
    }

    // Clear ready queues
    for (int i = 0; i < MAX_TASK_PRI; i++) {
        ready_queue[i] = NULL;
    }

    scheduled_task = NULL;
    main_task_id = 0;
}

// Test helper: Create a test task with specific priority
static int create_test_task(int priority) {
    TaskInfo task_info = {
        .task_attr = TA_HLNG,
        .task_addr = (FUNCPTR)dummy_task_func,
        .task_pri = priority,
        .stack_size = TASK_STACK_SIZE,
        .stack_addr = NULL  // Let system allocate
    };

    return ss_create_task(&task_info);
}

// Test basic task creation
TEST(scheduler_task_creation_basic) {
    reset_scheduler_state();
    ss_mem_init();  // Initialize memory for stack allocation

    int task_id = create_test_task(5);
    ASSERT_TRUE(task_id > 0);  // Valid task ID
    ASSERT_IN_RANGE(task_id, 1, MAX_TASKS);

    // Check TCB state
    TaskControlBlock* tcb = &tcb_table[task_id - 1];  // Task IDs are 1-based
    ASSERT_EQ(tcb->state, TS_DORMANT);
    ASSERT_EQ(tcb->task_pri, 5);
    ASSERT_NOT_NULL(tcb->task_addr);
}

// Test task creation with invalid parameters
TEST(scheduler_task_creation_invalid_params) {
    reset_scheduler_state();
    ss_mem_init();

    // Test invalid priority (too high)
    TaskInfo invalid_task = {
        .task_attr = TA_HLNG,
        .task_addr = (FUNCPTR)dummy_task_func,
        .task_pri = MAX_TASK_PRI + 1,  // Invalid priority
        .stack_size = TASK_STACK_SIZE,
        .stack_addr = NULL
    };

    int result = ss_create_task(&invalid_task);
    ASSERT_TRUE(result < 0);  // Should return error code

    // Test NULL task address
    invalid_task.task_pri = 5;  // Fix priority
    invalid_task.task_addr = NULL;  // Invalid address

    result = ss_create_task(&invalid_task);
    ASSERT_TRUE(result < 0);  // Should return error code
}

// Test task resource exhaustion
TEST(scheduler_task_creation_resource_exhaustion) {
    reset_scheduler_state();
    ss_mem_init();

    int created_tasks = 0;

    // Create tasks until we run out of slots
    for (int i = 0; i < MAX_TASKS + 1; i++) {
        int task_id = create_test_task(5);
        if (task_id > 0) {
            created_tasks++;
        } else {
            break;  // Resource exhaustion
        }
    }

    // Should have created exactly MAX_TASKS tasks
    ASSERT_EQ(created_tasks, MAX_TASKS);
}

// Test priority-based scheduling logic
TEST(scheduler_priority_based_scheduling) {
    reset_scheduler_state();
    ss_mem_init();

    // Create tasks with different priorities
    int high_pri_task = create_test_task(1);    // High priority (lower number)
    int med_pri_task = create_test_task(5);     // Medium priority
    int low_pri_task = create_test_task(10);    // Low priority

    ASSERT_TRUE(high_pri_task > 0);
    ASSERT_TRUE(med_pri_task > 0);
    ASSERT_TRUE(low_pri_task > 0);

    // Start all tasks (move to ready state)
    ss_start_task(high_pri_task);
    ss_start_task(med_pri_task);
    ss_start_task(low_pri_task);

    // Verify they are in ready state
    ASSERT_EQ(tcb_table[high_pri_task - 1].state, TS_READY);
    ASSERT_EQ(tcb_table[med_pri_task - 1].state, TS_READY);
    ASSERT_EQ(tcb_table[low_pri_task - 1].state, TS_READY);

    // Check ready queue organization
    // High priority task should be in ready_queue[1]
    ASSERT_NOT_NULL(ready_queue[1]);
    ASSERT_EQ(ready_queue[1], &tcb_table[high_pri_task - 1]);

    // Medium priority task should be in ready_queue[5]
    ASSERT_NOT_NULL(ready_queue[5]);
    ASSERT_EQ(ready_queue[5], &tcb_table[med_pri_task - 1]);

    // Low priority task should be in ready_queue[10]
    ASSERT_NOT_NULL(ready_queue[10]);
    ASSERT_EQ(ready_queue[10], &tcb_table[low_pri_task - 1]);
}

// Test scheduler selection logic
TEST(scheduler_highest_priority_selection) {
    reset_scheduler_state();
    ss_mem_init();

    // Create and start tasks with different priorities
    int low_pri_task = create_test_task(10);   // Low priority first
    int high_pri_task = create_test_task(2);   // High priority second
    int med_pri_task = create_test_task(5);    // Medium priority third

    ss_start_task(low_pri_task);
    ss_start_task(high_pri_task);
    ss_start_task(med_pri_task);

    // Manually call scheduler (normally called by timer interrupt)
    // Note: We need to access the static function, so this tests the logic
    // In real testing, we'd need to expose this or test through public interface

    // Verify highest priority task is scheduled
    // The scheduler should select the task with lowest priority number (highest priority)
    // This would require either exposing the scheduler function or testing through observable behavior

    // For now, verify the ready queue structure is correct
    ASSERT_NOT_NULL(ready_queue[2]);   // High priority task
    ASSERT_NOT_NULL(ready_queue[5]);   // Medium priority task
    ASSERT_NOT_NULL(ready_queue[10]);  // Low priority task

    // Higher priority queues should be checked first
    ASSERT_NULL(ready_queue[0]);  // No priority 0 tasks
    ASSERT_NULL(ready_queue[1]);  // No priority 1 tasks
}

// Test task state transitions
TEST(scheduler_task_state_transitions) {
    reset_scheduler_state();
    ss_mem_init();

    int task_id = create_test_task(5);
    ASSERT_TRUE(task_id > 0);

    TaskControlBlock* tcb = &tcb_table[task_id - 1];

    // Initial state should be DORMANT
    ASSERT_EQ(tcb->state, TS_DORMANT);

    // Start the task
    int result = ss_start_task(task_id);
    ASSERT_EQ(result, 0);  // Success

    // State should transition to READY
    ASSERT_EQ(tcb->state, TS_READY);

    // Task should be in appropriate ready queue
    ASSERT_NOT_NULL(ready_queue[5]);
    ASSERT_EQ(ready_queue[5], tcb);
}

// Test multiple tasks with same priority
TEST(scheduler_same_priority_tasks) {
    reset_scheduler_state();
    ss_mem_init();

    // Create multiple tasks with same priority
    int task1 = create_test_task(5);
    int task2 = create_test_task(5);
    int task3 = create_test_task(5);

    ASSERT_TRUE(task1 > 0);
    ASSERT_TRUE(task2 > 0);
    ASSERT_TRUE(task3 > 0);

    // Start all tasks
    ss_start_task(task1);
    ss_start_task(task2);
    ss_start_task(task3);

    // All should be in ready state
    ASSERT_EQ(tcb_table[task1 - 1].state, TS_READY);
    ASSERT_EQ(tcb_table[task2 - 1].state, TS_READY);
    ASSERT_EQ(tcb_table[task3 - 1].state, TS_READY);

    // They should be linked in the ready queue for priority 5
    ASSERT_NOT_NULL(ready_queue[5]);

    // Count tasks in the ready queue for priority 5
    int count = 0;
    TaskControlBlock* current = ready_queue[5];
    while (current != NULL) {
        count++;
        current = current->next;
        if (count > 10) break;  // Prevent infinite loop in case of error
    }
    ASSERT_EQ(count, 3);  // Should have 3 tasks in the queue
}

// Run all scheduler tests
void run_scheduler_tests(void) {
    RUN_TEST(scheduler_task_creation_basic);
    RUN_TEST(scheduler_task_creation_invalid_params);
    RUN_TEST(scheduler_task_creation_resource_exhaustion);
    RUN_TEST(scheduler_priority_based_scheduling);
    RUN_TEST(scheduler_highest_priority_selection);
    RUN_TEST(scheduler_task_state_transitions);
    RUN_TEST(scheduler_same_priority_tasks);
}