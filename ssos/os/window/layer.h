#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "ss_config.h"

enum LayerAttr {
    LAYER_ATTR_USED = 0x01,
    LAYER_ATTR_VISIBLE = 0x02,
};

typedef struct {
    uint16_t x, y, z;
    uint16_t w, h;
    uint16_t attr;
    uint8_t* vram;
    // Dirty rectangle tracking for performance
    uint16_t dirty_x, dirty_y, dirty_w, dirty_h;
    uint8_t needs_redraw;
} Layer;

typedef struct {
    uint8_t* src;
    uint8_t* dst;
    uint16_t count;
} BatchTransfer;

typedef struct {
    uint8_t* buffer;
    uint16_t width;
    uint16_t height;
    bool in_use;
} LayerBuffer;

typedef struct {
    int16_t topLayerIdx;
    Layer* zLayers[MAX_LAYERS]; // z-order sorted layer pointers
    Layer layers[MAX_LAYERS];   // layer data ( sizeof(Layer) * MAX_LAYERS )
    uint8_t* map;
    // Batch DMA optimization
    BatchTransfer batch_transfers[SS_CONFIG_DMA_MAX_TRANSFERS];
    int batch_count;
    bool batch_optimized;
    // Buffer pool for memory efficiency
    LayerBuffer buffer_pool[MAX_LAYERS];
    int buffer_pool_count;
    // Phase 6: Staged initialization and low clock optimization
    bool staged_init;           // Staged initialization flag
    bool low_clock_mode;        // Low clock mode flag
    bool buffers_initialized;   // Buffer initialization flag
    uint32_t timer_counter;     // Performance timer
    uint32_t batch_threshold;   // DMA batch threshold
    bool initialized;           // Initialization flag
} LayerMgr;

extern LayerMgr* ss_layer_mgr;

void ss_layer_init();
Layer* ss_layer_get();
Layer* ss_layer_get_with_buffer(uint16_t width, uint16_t height);
void ss_layer_set(Layer* layer, uint8_t* vram, uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h);
void ss_layer_set_z(Layer* layer, uint16_t z);
void ss_all_layer_draw();
void ss_all_layer_draw_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ss_layer_draw_rect_layer_dma(Layer* l, uint8_t* src, uint8_t* dst,
                                  uint16_t block_count);
void ss_layer_draw_rect_layer(Layer* l);
void ss_layer_move(Layer* layer, uint16_t x, uint16_t y);
void ss_layer_invalidate(Layer* layer);
void ss_layer_mark_dirty(Layer* layer, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ss_layer_mark_clean(Layer* layer);
void ss_layer_draw_dirty_only();
void ss_layer_update_map(Layer* layer);

// Batch DMA optimization functions
void ss_layer_init_batch_transfers(void);
void ss_layer_add_batch_transfer(uint8_t* src, uint8_t* dst, uint16_t count);
void ss_layer_execute_batch_transfers(void);
void ss_layer_draw_rect_layer_batch(Layer* l);

// Buffer pool management for memory efficiency
void ss_layer_init_buffer_pool(void);
uint8_t* ss_layer_alloc_buffer(uint16_t width, uint16_t height);
void ss_layer_free_buffer(uint8_t* buffer);
LayerBuffer* ss_layer_find_buffer(uint8_t* buffer);
void ss_layer_optimize_buffer_usage(void);

// Memory map optimization for Phase 2 performance improvement
void ss_layer_init_fast_map(void);
uint32_t ss_compute_layer_mask_fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
uint8_t ss_get_map_value_optimized(uint16_t x, uint16_t y);

// Superscalar transfer optimization for Phase 2
void ss_layer_draw_rect_layer_cpu_superscalar(uint8_t* src, uint8_t* dst, uint16_t count);
void ss_ultra_fast_copy_32bit(uint32_t* dst, uint32_t* src, int count);

// Double buffering system for Phase 2
void ss_layer_init_double_buffer(void);
void ss_layer_draw_to_backbuffer(Layer* l);
void ss_layer_flip_buffers(void);

// Internal cache functions
uint8_t ss_get_cached_map_value(uint16_t index);

// Phase 4: Advanced performance monitoring system
typedef struct {
    uint32_t total_layers_created;
    uint32_t total_layers_destroyed;
    uint32_t total_buffer_allocations;
    uint32_t total_buffer_frees;
    uint32_t total_dma_transfers;
    uint32_t total_cpu_transfers;
    uint32_t peak_memory_usage;
    uint32_t current_memory_usage;
    uint32_t average_frame_time;
    uint32_t frame_count;
    uint32_t last_monitor_time;
} PerformanceStats;

extern PerformanceStats ss_perf_stats;

void ss_reset_performance_stats(void);
void ss_get_performance_stats(PerformanceStats* stats);
void ss_record_frame_time(uint32_t frame_time);
void ss_perform_memory_defragmentation(void);
void ss_sort_buffer_pool_by_size(void);
void ss_update_statistics(void);

// Internal functions for batch optimization
void ss_layer_draw_rect_layer_batch_internal(Layer* l, int16_t dx0, int16_t dy0, int16_t dx1, int16_t dy1);
void ss_layer_draw_rect_layer_bounds_batch_internal(Layer* l, uint16_t dx0, uint16_t dy0, uint16_t dx1, uint16_t dy1);
void ss_execute_conditional_batch_transfers(void);
void ss_execute_batch_group(BatchTransfer* group, int count);
void ss_sort_batch_transfers_by_dst(void);
void ss_layer_draw_rect_layer_cpu_optimized_fallback(uint8_t* src, uint8_t* dst, uint16_t count);

// Phase 6: Staged initialization and low clock optimization
void ss_layer_init_staged(void);
void ss_layer_init_map_on_demand(void);
void ss_layer_init_map_fast(void);
void ss_layer_init_buffers_on_demand(void);
LayerBuffer* ss_layer_get_buffer_staged(uint16_t width, uint16_t height);
uint32_t detect_cpu_clock_speed(void);
void enable_low_clock_optimizations(void);
Layer* ss_layer_get_first_optimized(void);
void ss_layer_prebuild_map_for_layer(Layer* l);
void ss_layer_optimize_first_draw(Layer* l);
void ss_preinit_dma_for_layer(Layer* l);
void ss_layer_prefetch_for_first_draw(Layer* l);

// Phase 6: Debug and monitoring functions
void ss_layer_print_performance_info(void);
void ss_layer_print_clock_info(void);
void ss_layer_print_optimization_status(void);
bool ss_layer_is_low_clock_mode(void);
uint32_t ss_layer_get_detected_clock(void);

// Phase 6: Adaptive threshold management
void ss_layer_set_adaptive_threshold(uint16_t threshold);
uint16_t ss_layer_get_adaptive_threshold(void);

// Phase 6: Test and verification functions
void ss_layer_test_optimizations(void);
void ss_layer_reset_optimization_stats(void);

// SX-Window互換のシンプルLayerシステム
void ss_layer_init_simple(void);
Layer* ss_layer_get_simple(void);
void ss_layer_draw_simple(void);
void ss_layer_draw_simple_layer(Layer* l);
void ss_layer_set_simple(Layer* layer, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ss_layer_blit_fast(Layer* l, uint16_t dx, uint16_t dy, uint16_t dw, uint16_t dh);
void ss_layer_draw_rect_layer_simple(Layer* l);
void ss_layer_benchmark_simple(void);
void ss_layer_report_memory_simple(void);
void ss_layer_simple_mark_dirty(Layer* layer, bool include_lower_layers);
void ss_layer_simple_mark_rect(Layer* layer, uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h);
