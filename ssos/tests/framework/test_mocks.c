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

// Test constants for LOCAL_MODE
#ifdef LOCAL_MODE
#ifndef WIDTH
#define WIDTH 768
#endif
#ifndef HEIGHT
#define HEIGHT 512
#endif
#ifndef VRAMWIDTH
#define VRAMWIDTH 768
#endif
#ifndef vram_start
void* vram_start = (void*)0x00c00000;
#endif
#endif

// Global variables for testing
uint32_t ss_timerd_counter = 0;
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