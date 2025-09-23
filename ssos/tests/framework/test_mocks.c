/**
 * @file test_mocks.c
 * @brief SSOSテスト用のモック関数
 *
 * ユニットテストで使用するモック関数を提供。
 */

#include <stdint.h>
#include <stdlib.h>

// Test memory layout variables for LOCAL_MODE
#ifdef LOCAL_MODE
void* local_ssos_memory_base = (void*)0x10000000;
uint32_t local_ssos_memory_size = 1024 * 1024;  // 1MB
uint32_t local_text_size = 128 * 1024;          // 128KB
uint32_t local_data_size = 32 * 1024;           // 32KB
uint32_t local_bss_size = 64 * 1024;            // 64KB
#endif

// Add missing constants for testing
#ifndef SS_CONFIG_VRAM_WIDTH
#define SS_CONFIG_VRAM_WIDTH 1024
#endif
#ifndef SS_CONFIG_VRAM_HEIGHT
#define SS_CONFIG_VRAM_HEIGHT 1024
#endif
#ifndef SS_CONFIG_DISPLAY_WIDTH
#define SS_CONFIG_DISPLAY_WIDTH 768
#endif
#ifndef SS_CONFIG_DISPLAY_HEIGHT
#define SS_CONFIG_DISPLAY_HEIGHT 512
#endif
#ifndef SS_CONFIG_COLOR_FOREGROUND
#define SS_CONFIG_COLOR_FOREGROUND 15
#endif
#ifndef SS_CONFIG_COLOR_BACKGROUND
#define SS_CONFIG_COLOR_BACKGROUND 10
#endif
#ifndef SS_CONFIG_COLOR_TASKBAR
#define SS_CONFIG_COLOR_TASKBAR 14
#endif
#ifndef MFP_ADDRESS
#define MFP_ADDRESS 0xe88001
#endif
#ifndef VSYNC_BIT
#define VSYNC_BIT 0x10
#endif
#ifndef ESC_SCANCODE
#define ESC_SCANCODE 0x011b
#endif
#ifndef KEY_BUFFER_SIZE
#define KEY_BUFFER_SIZE 32
#endif

// Provide kernel constant definitions for native tests
#undef VRAMWIDTH
#undef VRAMHEIGHT
#undef WIDTH
#undef HEIGHT
#undef vram_start

const int VRAMWIDTH = 768;
const int VRAMHEIGHT = 512;
const int WIDTH = 768;
const int HEIGHT = 512;
const uint16_t color_fg = 15;
const uint16_t color_bg = 10;
const uint16_t color_tb = 14;

static uint16_t mock_vram_storage[768 * 512 / 2];
uint16_t* vram_start = mock_vram_storage;

// Global variables for testing
volatile uint32_t ss_timera_counter = 0;
volatile uint32_t ss_timerb_counter = 0;
volatile uint32_t ss_timerc_counter = 0;
volatile uint32_t ss_timerd_counter = 0;
volatile uint32_t ss_key_counter = 0;
volatile uint32_t ss_context_switch_counter = 0;
volatile uint32_t ss_save_data_base = 0;
// Note: global_counter is defined in task_manager.c
extern uint32_t global_counter;

// Mock functions for interrupt handling
void disable_interrupts(void) {
    // Do nothing in test environment
}

void enable_interrupts(void) {
    // Do nothing in test environment
}

// Mock main function
void ssosmain(void) {
    // Do nothing - main function is called from test runner
}

// Mock DMA functions
void* xfr_inf = NULL;

void dma_clear(void) {
    // Do nothing in test environment
}

void dma_init(void) {
    // Do nothing in test environment
}

void dma_start(void) {
    // Do nothing in test environment
}

void dma_wait_completion(void) {
    // Do nothing in test environment
}

// モック関数の実装
void* mock_malloc(size_t size) {
    return malloc(size);
}

void mock_free(void* ptr) {
    free(ptr);
}

// X68000 I/Oコールシステムのモック
int32_t iocs_call(uint16_t func_code, ...) {
    // テストでは何もしない
    return 0;
}

// VRAM関連のモック
void* mock_vram_buffer = NULL;

void mock_vram_init(void) {
    // 何もしない
}

void mock_vram_write(uint32_t offset, uint16_t value) {
    // 何もしない
}

uint16_t mock_vram_read(uint32_t offset) {
    return 0;
}