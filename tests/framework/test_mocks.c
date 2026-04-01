#include "ssos_test.h"

/* Shared mocks - available for both NATIVE_TEST and cross-compile */

volatile uint32_t ss_timerd_counter_val = 0;

#define ss_timerd_counter ss_timerd_counter_unused
#include "memory.h"
#include "ss_config.h"
#include "task_manager.h"
#undef ss_timerd_counter

volatile uint32_t ss_timerd_counter = 0;

const int VRAMWIDTH = SS_CONFIG_VRAM_WIDTH;
const int VRAMHEIGHT = SS_CONFIG_VRAM_HEIGHT;
const int WIDTH = SS_CONFIG_DISPLAY_WIDTH;
const int HEIGHT = SS_CONFIG_DISPLAY_HEIGHT;
const uint16_t color_fg = SS_CONFIG_COLOR_FOREGROUND;
const uint16_t color_bg = SS_CONFIG_COLOR_BACKGROUND;
const uint16_t color_tb = SS_CONFIG_COLOR_TASKBAR;

const volatile uint32_t ss_context_switch_counter = 0;

uint16_t vram_start_buffer[1024 * 1024];
uint16_t* vram_start = vram_start_buffer;

void dma_prepare_x68k_16color(void) {}
void dma_clear(void) {}
void dma_setup_span(void* dst, void* src, uint16_t count) {
    (void)dst;
    (void)src;
    (void)count;
}
void dma_start(void) {}
void dma_wait_completion(void) {}

typedef struct {
    uint32_t total_regions_processed;
    uint32_t total_pixels_drawn;
    uint32_t dma_transfers_count;
    uint32_t cpu_transfers_count;
    uint32_t last_report_time;
    uint32_t occlusion_culled_regions;
} DamagePerfStats_Mock;

DamagePerfStats_Mock g_damage_perf;

void disable_interrupts(void) {}
void enable_interrupts(void) {}
void ssosmain(void) {}

void* local_ssos_memory_base = NULL;
uint32_t local_ssos_memory_size = 1024 * 1024;
uint32_t local_text_size = 0, local_data_size = 0, local_bss_size = 0;

struct KeyBuffer ss_kb;

const volatile uint32_t ss_timera_counter = 0;
const volatile uint32_t ss_timerb_counter = 0;
const volatile uint32_t ss_timerc_counter = 0;
const volatile uint32_t ss_key_counter = 0;
const volatile uint32_t ss_save_data_base = 0;

void advance_timer_counter(uint32_t ticks) { ss_timerd_counter += ticks; }

void reset_scheduler_state(void) {
    int i;
    for (i = 0; i < MAX_TASKS; i++) {
        tcb_table[i].state = TS_NONEXIST;
        tcb_table[i].prev = NULL;
        tcb_table[i].next = NULL;
    }
    for (i = 0; i < MAX_TASK_PRI; i++) {
        ready_queue[i] = NULL;
    }
    curr_task = NULL;
    scheduled_task = NULL;
    global_counter = 0;
}

#ifdef NATIVE_TEST
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__attribute__((constructor)) void init_test_mocks(void) {
    local_ssos_memory_base = malloc(local_ssos_memory_size);
    if (!local_ssos_memory_base) {
        fprintf(stderr, "Failed to allocate mock memory region\n");
        exit(1);
    }
}
#endif
