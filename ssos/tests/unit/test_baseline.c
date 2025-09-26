/**
 * Phase 2: Baseline mode test
 * Test for identifying actual bottlenecks in 10MHz environment
 */

#include <stdio.h>
#include <stdint.h>
#include "../framework/ssos_test.h"
#include "../../os/window/layer.h"
#include "../../os/kernel/kernel.h"
#include "../../os/kernel/memory.h"

// Test constants
#define TEST_ITERATIONS 100
#define TEST_LAYER_COUNT 5

// ベースラインモードのパフォーマンス測定
static void test_baseline_initialization(void) {
    printf("=== Baseline Mode Initialization Test ===\n");

    // ベースラインモードの初期化
    ss_layer_init_baseline();

    // Check that optimization level is 0
    ASSERT_EQ(ss_layer_mgr->optimization_level, 0);

    // Check that all optimizations are disabled
    ASSERT_FALSE(ss_layer_mgr->batch_optimized);
    ASSERT_FALSE(ss_layer_mgr->staged_init);
    ASSERT_FALSE(ss_layer_mgr->low_clock_mode);

    printf("PASS: Baseline mode initialization successful\n");
}

// ベースラインモードでのメモリ使用量テスト
static void test_baseline_memory_usage(void) {
    printf("=== Baseline Mode Memory Usage Test ===\n");

    // ベースラインモードで複数レイヤーを作成
    Layer* layers[TEST_LAYER_COUNT];
    uint32_t start_memory = 0; // TODO: メモリ使用量取得関数の実装

    for (int i = 0; i < TEST_LAYER_COUNT; i++) {
        layers[i] = ss_layer_get_baseline();
        ASSERT_NOT_NULL(layers[i]);
    }

    // メモリ使用量の測定
    uint32_t end_memory = 0; // TODO: メモリ使用量取得関数の実装
    uint32_t memory_used = end_memory - start_memory;

    printf("Memory used for %d layers: %lu bytes\n", TEST_LAYER_COUNT, memory_used);

    // Clean up layers
    for (int i = 0; i < TEST_LAYER_COUNT; i++) {
        layers[i]->attr = 0;
    }
    ss_layer_mgr->topLayerIdx = 0;

    printf("PASS: Memory usage test completed\n");
}

// ベースラインモードでの描画パフォーマンステスト
static void test_baseline_drawing_performance(void) {
    printf("=== Baseline Mode Drawing Performance Test ===\n");

    // Create test layer
    Layer* test_layer = ss_layer_get_baseline();
    ASSERT_NOT_NULL(test_layer);

    // Allocate VRAM for testing
    test_layer->vram = (uint8_t*)ss_mem_alloc4k(64 * 64); // 64x64 pixel test area
    ASSERT_NOT_NULL(test_layer->vram);

    // Set up layer
    ss_layer_set(test_layer, test_layer->vram, 0, 0, 64, 64);

    // 描画パフォーマンスの測定
    uint32_t start_time = ss_timerd_counter;

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        // レイヤーの描画を実行
        ss_layer_draw_rect_layer(test_layer);
    }

    uint32_t end_time = ss_timerd_counter;
    uint32_t total_time = end_time - start_time;
    float avg_time = (float)total_time / TEST_ITERATIONS;

    printf("Average drawing time per frame: %.2f cycles\n", avg_time);
    printf("Total time for %d iterations: %lu cycles\n", TEST_ITERATIONS, total_time);

    // Clean up
    ss_mem_free((uint32_t)(uintptr_t)test_layer->vram, 0);
    test_layer->attr = 0;
    ss_layer_mgr->topLayerIdx = 0;

    printf("PASS: Drawing performance test completed\n");
}

// ベースライン状態の表示テスト
static void test_baseline_status_display(void) {
    printf("=== Baseline Mode Status Display Test ===\n");

    // Display baseline status
    ss_layer_print_baseline_status();

    printf("PASS: Status display test completed\n");
}

// ベースラインモードの有効化/無効化テスト
static void test_baseline_mode_switching(void) {
    printf("=== Baseline Mode Switching Test ===\n");

    // Enable baseline mode
    ss_layer_enable_baseline_mode();
    ASSERT_EQ(ss_layer_mgr->optimization_level, 0);

    // Disable baseline mode
    ss_layer_disable_baseline_mode();
    ASSERT_EQ(ss_layer_mgr->optimization_level, 7);

    printf("PASS: Mode switching test completed\n");
}

// ベースライン実装の包括的テスト
void test_baseline_implementation(void) {
    printf("\n=== Phase 2 Baseline Implementation Test Suite ===\n");

    // Run all tests
    test_baseline_initialization();
    test_baseline_memory_usage();
    test_baseline_drawing_performance();
    test_baseline_status_display();
    test_baseline_mode_switching();

    printf("\n=== All Baseline Tests Completed Successfully ===\n");
}

// 10MHz環境でのボトルネック特定テスト
void test_10mhz_bottlenecks(void) {
    printf("\n=== 10MHz Environment Bottleneck Analysis ===\n");

    // クロック速度の検出
    uint32_t detected_clock = detect_cpu_clock_speed();
    printf("Detected CPU clock: %lu MHz\n", detected_clock / 1000000);

    // ベースラインモードの有効化
    ss_layer_enable_baseline_mode();

    // 10MHz特有のボトルネックテスト
    if (detected_clock < 15000000) {  // 15MHz未満の場合
        printf("Running 10MHz-specific bottleneck tests...\n");

        // メモリ初期化速度テスト
        uint32_t start_time = ss_timerd_counter;
        ss_layer_init_baseline();
        uint32_t init_time = ss_timerd_counter - start_time;

        printf("Baseline initialization time: %lu cycles\n", init_time);

        // 連続メモリアクセスのパフォーマンステスト
        uint32_t memory_test_start = ss_timerd_counter;
        for (int i = 0; i < 1000; i++) {
            // メモリマップへの連続アクセス（10MHzでのボトルネック要因）
            ss_layer_mgr->map[i % 1000] = 1;
        }
        uint32_t memory_test_time = ss_timerd_counter - memory_test_start;

        printf("Memory access test time: %lu cycles\n", memory_test_time);
        printf("Average memory access time: %.2f cycles per access\n",
               (float)memory_test_time / 1000);

        // DMA転送のオーバーヘッドテスト
        // TODO: DMA初期化オーバーヘッドの測定

    } else {
        printf("System clock is %lu MHz - not in 10MHz range\n", detected_clock / 1000000);
    }

    printf("=== Bottleneck Analysis Completed ===\n");
}