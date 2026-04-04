#include "ssosmain.h"
#include "kernel.h"
#include "layer.h"
#include "memory.h"
#include "printf.h"
#include "ss_perf.h"
#include "ssoswindows.h"
#include "task_manager.h"
#include "vram.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x68k/iocs.h>

#include "damage.h"

// ============================================================
// Preemptive multitasking demo task functions
// Each task displays a counter at a different screen position
// ============================================================

// Task 1: Simple counter loop (no IOCS)
void task1_func(int16_t stacd, void* exinf) {
    (void)stacd;
    (void)exinf;
    volatile uint32_t counter = 0;
    while (1) {
        counter++; // Just increment, no IOCS
    }
}

// Task 2: Displays counter at Y=100
void task2_func(int16_t stacd, void* exinf) {
    (void)stacd;
    (void)exinf;
    uint32_t counter = 0;
    while (1) {
        _iocs_b_locate(0, 4);
        char buf[32];
        sprintf(buf, "Task2: %08lx", counter++);
        _iocs_b_print(buf);

        for (volatile int i = 0; i < 50000; i++) {
        }
    }
}

// Task 3: Displays counter at Y=150
void task3_func(int16_t stacd, void* exinf) {
    (void)stacd;
    (void)exinf;
    uint32_t counter = 0;
    while (1) {
        _iocs_b_locate(0, 5);
        char buf[32];
        sprintf(buf, "Task3: %08lx", counter++);
        _iocs_b_print(buf);

        for (volatile int i = 0; i < 50000; i++) {
        }
    }
}

void ssosmain() {
    int c;
    int scancode = 0;
    char szMessage[256];
    uint32_t prev_counter = 0;

    // init OS
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen
    _iocs_g_clr_on(); // clear gvram, reset palette, access page 0
    _iocs_b_curoff(); // stop cursor

    _iocs_ms_init();
    _iocs_skey_mod(0, 0, 0);
    _iocs_ms_curon();

    ss_clear_vram_fast();
    ss_wait_for_clear_vram_completion();

    ss_mem_init_info();
    ss_mem_init();

    // Initialize performance monitoring system
    ss_perf_init();

    // Initialize damage buffer system for dirty region tracking
    ss_damage_init();

    ss_task_stack_base = (uint8_t*)ss_mem_alloc4k(4096 * MAX_TASKS);

    // initialize tcb_table
    for (uint16_t i = 0; i < MAX_TASKS; i++) {
        tcb_table[i].state = TS_NONEXIST;
    }

    // Setup main task (the one already running)
    // Create a TCB for the main task so it can be managed by the scheduler
    // Use a dummy function pointer — the main task is already executing,
    // so this value will never be called via cs_start_new_task.
    static TaskInfo main_task;
    memset(&main_task, 0, sizeof(TaskInfo));
    main_task.task =
        (FUNCPTR)1; // Dummy non-NULL value (main task is already running)
    main_task.task_pri = 1; // Highest priority for the main task
    main_task.stack_size = TASK_STACK_SIZE;
    main_task.task_attr = TA_HLNG | TA_RNG0;

    // We MUST switch the current stack pointer to the new stack area
    // to avoid potential collisions with the program code/data.
    // Human68k's default stack might be too close to our code.
    uint32_t stack_end = (uint32_t)(ss_task_stack_base + TASK_STACK_SIZE);
    uint8_t* new_sp = (uint8_t*)((stack_end - 4) & 0xFFFFFFFC);
    static uint8_t* ss_old_main_sp = NULL;

    __asm__ __volatile__("move.l %%sp, %0\n"
                         "move.l %1, %%sp\n"
                         : "=r"(ss_old_main_sp)
                         : "r"(new_sp));

    main_task_id = ss_create_task(&main_task);
    curr_task = &tcb_table[main_task_id - 1];
    curr_task->state = TS_READY;
    curr_task->context = NULL; // NULL means "running, context not yet saved"

    // Add main task to ready queue so the scheduler can switch back to it later
    ss_task_queue_add_entry(curr_task);

#if 1 // Preemptive multitasking demo - ENABLED for testing
    // Create demo tasks for multitasking test
    // Each task will run in an infinite loop
    // Timer interrupt will switch between main task and demo tasks
    TaskInfo task1_info = {
        .task_attr = TA_HLNG,
        .task = task1_func,
        .task_pri = 1, // Same priority as main task for round-robin
        .stack_size = TASK_STACK_SIZE,
        .stack = NULL,
    };

    TaskInfo task2_info = {
        .task_attr = TA_HLNG,
        .task = task2_func,
        .task_pri = 1, // Same priority as main task for round-robin
        .stack_size = TASK_STACK_SIZE,
        .stack = NULL,
    };

    TaskInfo task3_info = {
        .task_attr = TA_HLNG,
        .task = task3_func,
        .task_pri = 1, // Same priority as main task for round-robin
        .stack_size = TASK_STACK_SIZE,
        .stack = NULL,
    };

    uint16_t task1_id = ss_create_task(&task1_info);
    uint16_t task2_id = ss_create_task(&task2_info);
    uint16_t task3_id = ss_create_task(&task3_info);

    // Start demo tasks - all tasks for testing
    if (task1_id > 0)
        ss_start_task(task1_id, 0);
    if (task2_id > 0)
        ss_start_task(task2_id, 0);
    if (task3_id > 0)
        ss_start_task(task3_id, 0);
#endif // Preemptive multitasking demo

#if 1
    ss_layer_init();

    // make layers
    Layer* l1 = get_layer_1();
    Layer* l2 = get_layer_2();
    Layer* l3 = get_layer_3();

    // Initial setup: update content first
    update_layer_2(l2);
    update_layer_3(l3);

    // Initial complete draw - use layer system for full screen refresh
    ss_all_layer_draw();

    // Reset damage buffer after initial complete draw to clear any accumulated
    // regions
    ss_damage_reset();

    while (true) {
        // Performance monitoring: Start frame timing
        SS_PERF_START_MEASUREMENT(SS_PERF_FRAME_TIME);

        // if it's vsync, wait for display period
        while (!((*mfp) & 0x10)) {
            if (ss_handle_keys() == -1)
#ifdef LOCAL_MODE
                goto CLEANUP;
#else
                ;
#endif
        }
        // wait for vsync
        while ((*mfp) & 0x10) {
            if (ss_handle_keys() == -1)
#ifdef LOCAL_MODE
                goto CLEANUP;
#else
                ;
#endif
        }

        if (ss_handle_keys() == -1)
#ifdef LOCAL_MODE
            break;

        ;

#endif

        // Performance monitoring: Start layer update timing
        SS_PERF_START_MEASUREMENT(SS_PERF_LAYER_UPDATE);

        if (g_drag.is_dragging) {
            // === DRAG MODE: Update drag frame and maintain dotted border ===
            
            // 1. Update mouse and drag frame
            // This also adds the old frame position to damage buffer for automatic cleanup
            ss_drag_update_frame();
            
            // 2. Reduce UI update frequency during drag to save CPU for background restoration
            if ((ss_timerd_counter & 0x7) == 0) {
                update_layer_3(l3);
            }

            // 3. Draw dirty regions (this erases the old XOR border using background data)
            ss_damage_draw_regions();

            // 4. Draw new XOR drag border on VRAM (after everything else)
            if (g_drag.dragged_layer) {
                int phase = ss_timerd_counter / 2;
                ss_xor_dotted_rect_vram(g_drag.drag_frame_x, g_drag.drag_frame_y,
                                        g_drag.dragged_layer->w, g_drag.dragged_layer->h, phase);
            }
        } else {
            // === NORMAL MODE: Full layer updates ===

            // Phase 1: Update layer content
            update_layer_3(l3);
            if (ss_timerd_counter > prev_counter + 1000 ||
                ss_timerd_counter < prev_counter) {
                prev_counter = ss_timerd_counter;
                update_layer_2(l2);
            }

            // Phase 2: Composite dirty regions to VRAM
            SS_PERF_START_MEASUREMENT(SS_PERF_DRAW_TIME);
            ss_damage_draw_regions();
            SS_PERF_END_MEASUREMENT(SS_PERF_DRAW_TIME);
        }

        // Performance monitoring: End layer update timing
        SS_PERF_END_MEASUREMENT(SS_PERF_LAYER_UPDATE);

        // Performance monitoring: Display damage statistics every 5 seconds
        static uint32_t last_perf_report = 0;
        if (ss_timerd_counter > last_perf_report + 5000) {
            last_perf_report = ss_timerd_counter;
            // In a real implementation, you might want to display these stats
            // ss_damage_perf_report();
        }

        // Performance monitoring: End frame timing
        SS_PERF_END_MEASUREMENT(SS_PERF_FRAME_TIME);
    }

CLEANUP:
    ss_damage_cleanup(); // Cleanup damage buffer
    // uninit
    _iocs_ms_curof();
    _iocs_skey_mod(-1, 0, 0);
    _iocs_b_curon();  // enable cursor
    _iocs_g_clr_on(); // clear graphics, reset palette to the default,
                      // access page 0
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen

    // Restore original stack before returning to Human68k
    if (ss_old_main_sp != NULL) {
        __asm__ __volatile__("move.l %0, %%sp\n" : : "r"(ss_old_main_sp));
    }
#endif
}
