/**
 * @file test_mocks.c
 * @brief SSOSテスト用のモック関数
 *
 * ユニットテストで使用するモック関数を提供。
 */

#include <stdint.h>
#include <stdlib.h>

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