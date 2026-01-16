#include "ssos_test.h"

#ifdef NATIVE_TEST
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// 1. Define the real counter that we can modify
volatile uint32_t ss_timerd_counter = 0; 

// 2. Trick the headers into using a different name for their extern declaration
#define ss_timerd_counter ss_timerd_counter_unused
#include "ss_config.h"
#include "memory.h"
#include "task_manager.h"
#undef ss_timerd_counter

#define SS_CONFIG_VRAM_WIDTH  1024
#define SS_CONFIG_VRAM_HEIGHT 1024

// Constants from kernel.c needed for testing
const int VRAMWIDTH = SS_CONFIG_VRAM_WIDTH;
const int VRAMHEIGHT = SS_CONFIG_VRAM_HEIGHT;
const int WIDTH = SS_CONFIG_DISPLAY_WIDTH;
const int HEIGHT = SS_CONFIG_DISPLAY_HEIGHT;

// Mock context switch counter
const volatile uint32_t ss_context_switch_counter = 0;

// Timer helper for tests
void advance_timer_counter(uint32_t ticks) {
    ss_timerd_counter += ticks;
}

// Function pointer needed for task manager
uint16_t vram_start_buffer[1024*1024];
uint16_t* vram_start = vram_start_buffer;

// DMA Mocks (needed by layer.c)
void dma_prepare_x68k_16color(void) {}
void dma_clear(void) {}
void dma_setup_span(void* dst, void* src, uint16_t count) {}
void dma_start(void) {}
void dma_wait_completion(void) {}

// Performance data dummy
struct {
    uint32_t cpu_transfers_count;
    uint32_t dma_transfers_count;
    uint32_t total_regions_processed;
} g_damage_perf;

// Missing symbols for linking
void disable_interrupts(void) {}
void enable_interrupts(void) {}
void ssosmain(void) {}

// Mock memory region pointers
void* local_ssos_memory_base = NULL;
uint32_t local_ssos_memory_size = 1024 * 1024; // 1MB
uint32_t local_text_size = 0, local_data_size = 0, local_bss_size = 0;

__attribute__((constructor))
void init_test_mocks(void) {
    local_ssos_memory_base = malloc(local_ssos_memory_size);
    if (!local_ssos_memory_base) {
        fprintf(stderr, "Failed to allocate mock memory region\n");
        exit(1);
    }
}

// Initialize scheduler state
void reset_scheduler_state(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        memset(&tcb_table[i], 0, sizeof(TaskControlBlock));
        tcb_table[i].state = TS_NONEXIST;
    }
    for (int i = 0; i < MAX_TASK_PRI; i++) {
        ready_queue[i] = NULL;
    }
    curr_task = NULL;
    scheduled_task = NULL;
    global_counter = 0;
}

#endif