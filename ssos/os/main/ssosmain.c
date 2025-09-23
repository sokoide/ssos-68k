#include "ssosmain.h"
#include "kernel.h"
#include "layer.h"
#include "memory.h"
#include "printf.h"
#include "quickdraw.h"
#include "quickdraw_monitor.h"
#include "quickdraw_shell.h"
#include "ssoswindows.h"
#include "task_manager.h"
#include "vram.h"
#include "ss_perf.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x68k/iocs.h>

enum {
    SS_UI_MODE_LAYER = 0,
    SS_UI_MODE_QUICKDRAW = 1,
};

#ifndef SS_BOOT_UI_MODE
#define SS_BOOT_UI_MODE SS_UI_MODE_LAYER
#endif

static bool ss_escape_requested(void) {
    int result = ss_handle_keys();
#ifdef LOCAL_MODE
    return result == -1;
#else
    (void)result;
    return false;
#endif
}

static void ss_run_quickdraw_mode(void) {
    qd_init();
    qd_set_vram_buffer((uint8_t*)vram_start);
    ss_init_palette();

    // Initialize desktop chrome and monitor panel
    qd_shell_draw_desktop_chrome();
    qd_monitor_panel_init();

    // Performance measurement variables
    uint32_t prev_counter = 0;
    bool first_frame = true;

    while (true) {
        ss_wait_for_vsync();

        // Update taskbar equivalent (Layer 3) - every frame
        qd_shell_update_taskbar();

        // Update monitor panel (Layer 2) - every second
        if (ss_timerd_counter > prev_counter + 1000 ||
            ss_timerd_counter < prev_counter ||
            first_frame) {
            prev_counter = ss_timerd_counter;
            qd_monitor_panel_tick();
            first_frame = false;
        }

        // Handle escape key
        if (ss_escape_requested()) {
#ifdef LOCAL_MODE
            break;
#else
            ;
#endif
        }
    }
}

static void ss_run_layer_mode(void) {
    uint32_t prev_counter = 0;

    ss_layer_init();

    Layer* l1 = get_layer_1();
    Layer* l2 = get_layer_2();
    Layer* l3 = get_layer_3();
    (void)l1;

    ss_all_layer_draw();
    update_layer_2(l2);
    update_layer_3(l3);

    ss_layer_draw_dirty_only();

    while (true) {
        SS_PERF_START_MEASUREMENT(SS_PERF_FRAME_TIME);

        while (!((*mfp) & 0x10)) {
            if (ss_escape_requested()) {
#ifdef LOCAL_MODE
                return;
#else
                ;
#endif
            }
        }

        while ((*mfp) & 0x10) {
            if (ss_escape_requested()) {
#ifdef LOCAL_MODE
                return;
#else
                ;
#endif
            }
        }

        if (ss_escape_requested()) {
#ifdef LOCAL_MODE
            break;
#else
            ;
#endif
        }

        SS_PERF_START_MEASUREMENT(SS_PERF_LAYER_UPDATE);
        update_layer_3(l3);

        if (ss_timerd_counter > prev_counter + 1000 ||
            ss_timerd_counter < prev_counter) {
            prev_counter = ss_timerd_counter;
            update_layer_2(l2);
        }

        SS_PERF_END_MEASUREMENT(SS_PERF_LAYER_UPDATE);

        SS_PERF_START_MEASUREMENT(SS_PERF_DRAW_TIME);
        ss_layer_draw_dirty_only();
        SS_PERF_END_MEASUREMENT(SS_PERF_DRAW_TIME);

        SS_PERF_END_MEASUREMENT(SS_PERF_FRAME_TIME);
    }
}

static void ss_shutdown_display(void) {
    _iocs_ms_curof();
    _iocs_skey_mod(-1, 0, 0);
    _iocs_b_curon();
    _iocs_g_clr_on();
    _iocs_crtmod(16);
}

void ssosmain() {
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

    if (SS_BOOT_UI_MODE == SS_UI_MODE_QUICKDRAW) {
        ss_run_quickdraw_mode();
    } else {
        ss_run_layer_mode();
    }

    ss_shutdown_display();
}
