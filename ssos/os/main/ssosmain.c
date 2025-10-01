#include "ssosmain.h"
#include "kernel.h"
#include "layer.h"
#include "memory.h"
#include "printf.h"
#include "ssoswindows.h"
#include "task_manager.h"
#include "vram.h"
#include "ss_perf.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x68k/iocs.h>

#include "damage.h"

// DEBUG: Add damage system debugging
uint32_t debug_damage_count = 0;
uint32_t debug_occlusion_count = 0;

// External declarations for damage system debugging
extern void ss_debug_damage_info();
extern void ss_debug_occlusion_info();
extern void ss_debug_display_stats();

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

    ss_init_memory_info();
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

    main_task.stack = (uint8_t*)(ss_save_data_base * TASK_STACK_SIZE - 1);
    main_task_id = ss_create_task(&main_task);

#if 0
    ss_start_task(main_task_id, 0);

    while (1)
        ; // never reaches here
#endif

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
    
    // Reset damage buffer after initial complete draw to clear any accumulated regions
    ss_damage_reset();
    
    // DEBUG: Log damage buffer reset
    // debug_damage_count = 0; // Reset counter when damage buffer is reset
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
    update_layer_3(l3);

    if (ss_timerd_counter > prev_counter + 1000 ||
        ss_timerd_counter < prev_counter) {
        prev_counter = ss_timerd_counter;
        update_layer_2(l2);
    }

    // Performance monitoring: End layer update timing
    SS_PERF_END_MEASUREMENT(SS_PERF_LAYER_UPDATE);

    // OPTIMIZED: Use damage system with occlusion optimization disabled
    SS_PERF_START_MEASUREMENT(SS_PERF_DRAW_TIME);
    ss_damage_draw_regions();
    SS_PERF_END_MEASUREMENT(SS_PERF_DRAW_TIME);
    
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
    ss_damage_cleanup();  // Cleanup damage buffer
    // uninit
    _iocs_ms_curof();
    _iocs_skey_mod(-1, 0, 0);
    _iocs_b_curon();  // enable cursor
    _iocs_g_clr_on(); // clear graphics, reset palette to the default,
                      // access page 0
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen
#endif
}
