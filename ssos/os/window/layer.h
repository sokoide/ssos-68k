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
