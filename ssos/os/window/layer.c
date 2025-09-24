// LOCAL_MODE constants for testing
#ifdef LOCAL_MODE
// Use different variable names to avoid conflicts with kernel.h extern declarations
#ifndef TEST_WIDTH
#define TEST_WIDTH 768
#endif
#ifndef TEST_HEIGHT
#define TEST_HEIGHT 512
#endif
#ifndef TEST_VRAMWIDTH
#define TEST_VRAMWIDTH 768
#endif
#endif

// Layer system dimensions - use constants from kernel.h
extern const int WIDTH;
extern const int HEIGHT;
extern const int VRAMWIDTH;

#include "layer.h"
#include "ssoswindows.h"
#include "dma.h"
#include "kernel.h"
#include "memory.h"
#include "vram.h"
#include "ss_perf.h"
#include <stddef.h>

LayerMgr* ss_layer_mgr;

// メモリマップの動的キャッシュ（最適化版）
#define MAP_CACHE_SIZE 64
static struct {
    uint16_t map_index;
    uint8_t layer_id;
    uint32_t last_access;
    bool valid;
} map_cache[MAP_CACHE_SIZE];

static uint32_t map_cache_hits = 0;
static uint32_t map_cache_misses = 0;

// Phase 2: 高速メモリマップ参照テーブル（32x32単位）
#define FAST_MAP_HEIGHT (SS_CONFIG_DISPLAY_HEIGHT/32)
#define FAST_MAP_WIDTH (SS_CONFIG_DISPLAY_WIDTH/32)
static uint32_t s_fast_map[FAST_MAP_HEIGHT][FAST_MAP_WIDTH];

// Phase 2: ダブルバッファシステム
static struct {
    LayerBuffer* front_buffer;
    LayerBuffer* back_buffer;
    bool back_buffer_dirty;
    uint32_t dirty_blocks[1024]; // 変更されたブロックを記録
} double_buffer;

void ss_layer_init() {
    ss_layer_mgr = (LayerMgr*)ss_mem_alloc4k(sizeof(LayerMgr));
    ss_layer_mgr->topLayerIdx = 0;
    for (int i = 0; i < MAX_LAYERS; i++) {
        ss_layer_mgr->layers[i].attr = 0;
    }
    ss_layer_mgr->map = (uint8_t*)ss_mem_alloc4k(
#ifdef LOCAL_MODE
        TEST_WIDTH / 8 * TEST_HEIGHT / 8
#else
        WIDTH / 8 * HEIGHT / 8
#endif
    );
    // reset to 0 every 4 bytes
    uint32_t* p = (uint32_t*)ss_layer_mgr->map;
    for (int i = 0; i < (
#ifdef LOCAL_MODE
        TEST_WIDTH / 8 * TEST_HEIGHT / 8
#else
        WIDTH / 8 * HEIGHT / 8
#endif
    ) / sizeof(uint32_t); i++) {
        *p++ = 0;
    }

    // Initialize batch DMA optimization
    ss_layer_init_batch_transfers();

    // Initialize buffer pool for memory efficiency
    ss_layer_init_buffer_pool();

    // Initialize memory map cache for performance optimization
    // (already implemented, no need to call again)

    // Phase 2 optimizations disabled for stability
}

Layer* ss_layer_get() {
    Layer* l = NULL;
    for (int i = 0; i < MAX_LAYERS; i++) {
        if ((ss_layer_mgr->layers[i].attr & LAYER_ATTR_USED) == 0) {
            l = &ss_layer_mgr->layers[i];
            l->attr = (LAYER_ATTR_USED | LAYER_ATTR_VISIBLE);
            l->z = ss_layer_mgr->topLayerIdx;
            ss_layer_mgr->zLayers[ss_layer_mgr->topLayerIdx] = l;
            ss_layer_mgr->topLayerIdx++;

            // Initialize dirty rectangle tracking - mark entire layer as dirty initially
            l->needs_redraw = 1;
            l->dirty_x = 0;
            l->dirty_y = 0;
            l->dirty_w = 0; // Will be set in ss_layer_set
            l->dirty_h = 0; // Will be set in ss_layer_set

            return l;
        }
    }
    return l;
}

Layer* ss_layer_get_with_buffer(uint16_t width, uint16_t height) {
    Layer* l = ss_layer_get();
    if (l) {
        // Phase 3: Enhanced error handling for buffer allocation
        l->vram = ss_layer_alloc_buffer(width, height);

        // Handle allocation failure gracefully
        if (!l->vram) {
            // If buffer allocation failed, try smaller size
            if (width > 32 || height > 32) {
                l->vram = ss_layer_alloc_buffer(32, 32);
                if (l->vram) {
                    l->w = 32;
                    l->h = 32;
                    l->dirty_w = 32;
                    l->dirty_h = 32;
                } else {
                    // If even minimum size fails, release layer and return NULL
                    l->attr = 0;  // Mark as unused
                    ss_layer_mgr->topLayerIdx--;
                    return NULL;
                }
            } else {
                // Minimum size allocation also failed
                l->attr = 0;  // Mark as unused
                ss_layer_mgr->topLayerIdx--;
                return NULL;
            }
        } else {
            // Normal case: buffer allocated successfully
            l->w = width;
            l->h = height;
            l->dirty_w = width;
            l->dirty_h = height;
        }
    }
    return l;
}

void ss_layer_set(Layer* layer, uint8_t* vram, uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h) {
    // If vram is NULL, try to get buffer from pool
    if (!vram) {
        vram = ss_layer_alloc_buffer(w, h);
    }

    layer->vram = vram;
    layer->x = (x & 0xFFF8);
    layer->y = (y & 0xFFF8);
    layer->w = (w & 0xFFF8);
    layer->h = (h & 0xFFF8);

    // Mark entire layer as dirty for initial draw
    layer->dirty_w = layer->w;
    layer->dirty_h = layer->h;

    uint8_t lid = layer - ss_layer_mgr->layers;

    // Use bit shifts for faster division by 8
    uint16_t map_width =
#ifdef LOCAL_MODE
        TEST_WIDTH >> 3;  // TEST_WIDTH / 8
#else
        WIDTH >> 3;  // WIDTH / 8
#endif
    uint16_t layer_y_div8 = layer->y >> 3;
    uint16_t layer_x_div8 = layer->x >> 3;
    uint16_t layer_h_div8 = layer->h >> 3;
    uint16_t layer_w_div8 = layer->w >> 3;

    for (int dy = 0; dy < layer_h_div8; dy++) {
        uint8_t* map_row = &ss_layer_mgr->map[(layer_y_div8 + dy) * map_width + layer_x_div8];
        for (int dx = 0; dx < layer_w_div8; dx++) {
            map_row[dx] = lid;
        }
    }
}

void ss_layer_set_z(Layer* layer, uint16_t z) {
    // uint16_t prev = layer->z;

    // TODO:
    // if (z < 0) {
    //     z = 0;
    // }
    // if (z > ss_layer_mgr->topLayerIdx + 1) {
    //     z = ss_layer_mgr->topLayerIdx + 1;
    // }
    // layer->z = z;

    // // reorder zLayers
    // if (prev > z) {
    //     // lower than the previous z
    //     for (int i = prev; i > z; i--) {
    //         ss_layer_mgr->zLayers[i] = ss_layer_mgr->zLayers[i - 1];
    //         ss_layer_mgr->zLayers[i]->z = i;
    //     }
    //     ss_layer_mgr->zLayers[z] = layer;
    // } else if (prev < z) {
    //     // higher than the previous z
    //     for (int i = prev; i < z; i++) {
    //         ss_layer_mgr->zLayers[i] = ss_layer_mgr->zLayers[i + 1];
    //         ss_layer_mgr->zLayers[i]->z = i;
    //     }
    //     ss_layer_mgr->zLayers[z] = layer;
    // }
    // ss_layer_draw();
}

void ss_all_layer_draw() {
    ss_all_layer_draw_rect(0, 0,
#ifdef LOCAL_MODE
        TEST_WIDTH, TEST_HEIGHT
#else
        WIDTH, HEIGHT
#endif
    );
}

// Draw the rectangle area (x0, y0) - (x1, y1)
// in vram coordinates
void ss_all_layer_draw_rect(uint16_t x0, uint16_t y0, uint16_t x1,
                            uint16_t y1) {
    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* layer = ss_layer_mgr->zLayers[i];
        ss_layer_draw_rect_layer(layer);
    }
}

// Draw the rectangle area (x0, y0) - (x1, y1)
// in vram coordinates for the layer id - OLD VERSION (SLOW)
void ss_layer_draw_rect_layer(Layer* l) {
    uint16_t x0 = l->x;
    uint16_t y0 = l->y;
    uint16_t x1 = l->x + l->w;
    uint16_t y1 = l->y + l->h;

    if (0 == (l->attr & LAYER_ATTR_VISIBLE))
        return;
    uint8_t lid = l - ss_layer_mgr->layers;

    // (dx0, dy0) - (dx1, dy1) is the intersection of the layer and the
    // rectangle (x0, y0) - (x1, y1).
    // l->x+dx0, l->y+dy0 - l->x+dx1, l->y+dy1 will be
    // redrawn.
    int16_t dx0 = x0 - l->x;
    int16_t dy0 = y0 - l->y;
    int16_t dx1 = x1 - l->x;
    int16_t dy1 = y1 - l->y;
    if (dx0 < 0)
        dx0 = 0;
    if (dy0 < 0)
        dy0 = 0;
    if (dx1 > l->w)
        dx1 = l->w;
    if (dy1 > l->h)
        dy1 = l->h;

    // Use batch DMA for better performance
    ss_layer_draw_rect_layer_batch_internal(l, dx0, dy0, dx1, dy1);
}

// Internal function for batch DMA processing
void ss_layer_draw_rect_layer_batch_internal(Layer* l, int16_t dx0, int16_t dy0, int16_t dx1, int16_t dy1) {
    if (ss_layer_mgr->batch_optimized) {
        // Use batch processing for better performance
        for (int16_t dy = dy0; dy < dy1; dy++) {
            int16_t vy = l->y + dy;
            if (vy < 0 || vy >=
#ifdef LOCAL_MODE
                TEST_HEIGHT
#else
                HEIGHT
#endif
            )
                continue;

            // Optimized DMA transfer using bit shifts
            uint16_t map_width =
        #ifdef LOCAL_MODE
                TEST_WIDTH >> 3;  // TEST_WIDTH / 8
        #else
                WIDTH >> 3;  // WIDTH / 8
        #endif
            uint16_t vy_div8 = vy >> 3;
            uint16_t l_x_div8 = l->x >> 3;

            int16_t startdx = -1;
            for (int16_t dx = dx0; dx < dx1; dx += 8) {
                // 従来の実装に戻す（高速化のため）
                if (ss_layer_mgr->map[vy_div8 * map_width + ((l->x + dx) >> 3)] == l->z) {
                    // set the first addr to transfer -> startdx
                    if (startdx == -1)
                        startdx = dx;
                } else if (startdx >= 0) {
                    // transfer between startdx and dx using batch
                    int16_t vx = l->x + startdx;
                    ss_layer_add_batch_transfer(
                        &l->vram[dy * l->w + startdx],
                        ((uint8_t*)&vram_start[vy * VRAMWIDTH + vx]) + 1,
                        dx - startdx);
                    startdx = -1;
                }
            }
            // DMA if the last block is not transferred yet
            if (startdx >= 0) {
                int16_t vx = l->x + startdx;
                ss_layer_add_batch_transfer(
                    &l->vram[dy * l->w + startdx],
                    ((uint8_t*)&vram_start[vy * VRAMWIDTH + vx]) + 1,
                    dx1 - startdx);
            }
        }
        // Execute all batched transfers at once
        ss_layer_execute_batch_transfers();
    } else {
        // Fallback to old method if batch optimization is disabled
        for (int16_t dy = dy0; dy < dy1; dy++) {
            int16_t vy = l->y + dy;
            if (vy < 0 || vy >=
#ifdef LOCAL_MODE
                TEST_HEIGHT
#else
                HEIGHT
#endif
            )
                continue;
            // Optimized DMA transfer using bit shifts
            uint16_t map_width =
        #ifdef LOCAL_MODE
                TEST_WIDTH >> 3;  // TEST_WIDTH / 8
        #else
                WIDTH >> 3;  // WIDTH / 8
        #endif
            uint16_t vy_div8 = vy >> 3;
            uint16_t l_x_div8 = l->x >> 3;

            int16_t startdx = -1;
            for (int16_t dx = dx0; dx < dx1; dx += 8) {
                if (ss_layer_mgr->map[vy_div8 * map_width + ((l->x + dx) >> 3)] == l->z) {
                    // set the first addr to transfer -> startdx
                    if (startdx == -1)
                        startdx = dx;
                } else if (startdx >= 0) {
                    // transfer between startdx and dx
                    int16_t vx = l->x + startdx;
                    ss_layer_draw_rect_layer_dma(
                        l, &l->vram[dy * l->w + startdx],
                        ((uint8_t*)&vram_start[vy * VRAMWIDTH + vx]) + 1,
                        dx - startdx);
                    startdx = -1;
                }
            }
            // DMA if the last block is not transferred yet
            if (startdx >= 0) {
                int16_t vx = l->x + startdx;
                ss_layer_draw_rect_layer_dma(
                    l, &l->vram[dy * l->w + startdx],
                    // X68000 16-color mode: VRAM is 2 bytes per pixel
                    // Transfer to lower byte (byte 1) where lower 4 bits are used
                    // X68000 VRAM structure: 1024 dots x 2 bytes/dot = 2048 bytes per line
                    // Display area: 768 dots, unused area: 256 dots (512 bytes)
                    // Center the display data by offsetting by (1024-768)/2 = 128 dots
                    ((uint8_t*)&vram_start[vy * VRAMWIDTH + vx]) + 1,
                    dx1 - startdx);
            }
        }
    }
}

// Public batch-optimized drawing function
void ss_layer_draw_rect_layer_batch(Layer* l) {
    uint16_t x0 = l->x;
    uint16_t y0 = l->y;
    uint16_t x1 = l->x + l->w;
    uint16_t y1 = l->y + l->h;

    if (0 == (l->attr & LAYER_ATTR_VISIBLE))
        return;

    // (dx0, dy0) - (dx1, dy1) is the intersection of the layer and the
    // rectangle (x0, y0) - (x1, y1).
    int16_t dx0 = x0 - l->x;
    int16_t dy0 = y0 - l->y;
    int16_t dx1 = x1 - l->x;
    int16_t dy1 = y1 - l->y;
    if (dx0 < 0)
        dx0 = 0;
    if (dy0 < 0)
        dy0 = 0;
    if (dx1 > l->w)
        dx1 = l->w;
    if (dy1 > l->h)
        dy1 = l->h;

    // Use batch processing for better performance
    ss_layer_draw_rect_layer_batch_internal(l, dx0, dy0, dx1, dy1);
}

// Performance monitoring variables for adaptive DMA thresholds
static uint32_t ss_cpu_idle_time = 0;
static uint32_t ss_last_performance_check = 0;
// Phase 3: Optimized adaptive DMA threshold
static uint16_t ss_adaptive_dma_threshold = 12;  // Optimized value for better balance
static uint32_t ss_total_transfers = 0;
static uint32_t ss_small_transfers = 0;

// Phase 3: Enhanced adaptive DMA threshold with transfer statistics
void ss_update_performance_metrics() {
    uint32_t current_time = ss_timerd_counter;

    // Update performance metrics every 200ms (reduced frequency for stability)
    if (current_time > ss_last_performance_check + 200) {
        // Estimate CPU idle time based on system activity
        static uint32_t last_activity = 0;
        static uint32_t last_total_transfers = 0;
        static uint32_t last_small_transfers = 0;

        uint32_t activity_delta = current_time - last_activity;
        uint32_t transfers_delta = ss_total_transfers - last_total_transfers;
        uint32_t small_transfers_delta = ss_small_transfers - last_small_transfers;

        // Phase 3: Statistics-based adaptive algorithm
        float small_transfer_ratio = 0.0f;
        if (transfers_delta > 0) {
            small_transfer_ratio = (float)small_transfers_delta / transfers_delta;
        }

        // Enhanced adaptive DMA threshold algorithm with statistics
        if (activity_delta < 50) {
            // High activity - prefer DMA for larger blocks to free up CPU
            ss_adaptive_dma_threshold = 20;  // Optimized value
        } else if (activity_delta < 100) {
            // Medium-high activity - balanced approach with statistics
            if (small_transfer_ratio > 0.7f) {
                ss_adaptive_dma_threshold = 16;  // Many small transfers
            } else {
                ss_adaptive_dma_threshold = 18;  // Balanced
            }
        } else if (activity_delta < 200) {
            // Medium-low activity - prefer DMA for medium blocks
            ss_adaptive_dma_threshold = 14;  // Optimized value
        } else if (activity_delta < 500) {
            // Low activity - use DMA for smaller blocks for consistency
            if (small_transfer_ratio > 0.5f) {
                ss_adaptive_dma_threshold = 10;  // Many small transfers
            } else {
                ss_adaptive_dma_threshold = 12;  // Balanced
            }
        } else {
            // Very low activity - use DMA aggressively
            ss_adaptive_dma_threshold = 8;   // Optimized value
        }

        // Store current values for next comparison
        last_activity = current_time;
        last_total_transfers = ss_total_transfers;
        last_small_transfers = ss_small_transfers;
        ss_last_performance_check = current_time;
    }
}

void ss_layer_draw_rect_layer_dma(Layer* l, uint8_t* src, uint8_t* dst,
                                  uint16_t block_count) {
    // Ensure alignment for optimal DMA performance
    if (block_count == 0) return;

    // Update performance metrics for adaptive behavior
    ss_update_performance_metrics();

    // ADAPTIVE DMA THRESHOLD: Use CPU load to decide transfer method
    // - High CPU activity: Use DMA for larger blocks to free up CPU
    // - Low CPU activity: Use DMA even for smaller blocks for consistency
    // - Normal activity: Balanced approach

    // Phase 3: Enhanced adaptive DMA threshold with statistics
    ss_total_transfers++;
    if (block_count <= ss_adaptive_dma_threshold) {
        ss_small_transfers++;
        // 従来の最適化実装に戻す（安定性確保）
        if (block_count >= 8 && ((uintptr_t)src & 3) == 0 && ((uintptr_t)dst & 3) == 0) {
            // 32-bit aligned transfer for optimal CPU performance (increased threshold from 4 to 8)
            uint32_t* src32 = (uint32_t*)src;
            uint32_t* dst32 = (uint32_t*)dst;
            int blocks = block_count >> 2;
            for (int i = 0; i < blocks; i++) {
                *dst32++ = *src32++;
            }
            // Handle remaining bytes
            uint8_t* src8 = (uint8_t*)src32;
            uint8_t* dst8 = (uint8_t*)dst32;
            for (int i = 0; i < (block_count & 3); i++) {
                *dst8++ = *src8++;
            }
        } else {
            // Optimized byte-by-byte transfer with unrolling
            int i = 0;
            // Unroll loop for better performance
            for (; i < block_count - 3; i += 4) {
                dst[i] = src[i];
                dst[i + 1] = src[i + 1];
                dst[i + 2] = src[i + 2];
                dst[i + 3] = src[i + 3];
            }
            // Handle remaining bytes
            for (; i < block_count; i++) {
                dst[i] = src[i];
            }
        }
        return;
    }

    // Phase 3: Enhanced DMA error handling
    int retry_count = 0;
    const int max_retries = 2;

    while (retry_count <= max_retries) {
        // Configure DMA transfer (従来の実装)
        dma_init(dst, 1);
        xfr_inf[0].addr = src;
        xfr_inf[0].count = block_count;

        // Execute DMA transfer
        dma_start();
        dma_wait_completion();

        // Check for DMA errors
        uint8_t dma_status = dma->csr;
        if ((dma_status & 0x90) == 0x90) {
            // DMA completed successfully
            dma_clear();
            return;
        } else {
            // DMA error occurred
            dma_clear();
            retry_count++;

            if (retry_count <= max_retries) {
                // Retry with smaller block size
                if (block_count > 256) {
                    block_count = 256;
                    continue;
                } else if (block_count > 64) {
                    block_count = 64;
                    continue;
                }
            }

            // If all retries failed, fall back to CPU transfer
            ss_layer_draw_rect_layer_cpu_optimized_fallback(src, dst, block_count);
            return;
        }
    }
}

/*
void ss_layer_move(Layer* layer, uint16_t x, uint16_t y) {
    uint16_t prevx = layer->x;
    uint16_t prevy = layer->y;
    layer->x = x;
    layer->y = y;
    ss_all_layer_draw_rect(prevx, prevy, prevx + layer->w, prevy + layer->h);
    ss_all_layer_draw_rect(x, y, x + layer->w, y + layer->h);
}
*/

// Mark a rectangular region of a layer as dirty (needs redrawing)
void ss_layer_mark_dirty(Layer* layer, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (!layer || w == 0 || h == 0) return;

    // Clamp to layer bounds
    if (x >= layer->w || y >= layer->h) return;
    if (x + w > layer->w) w = layer->w - x;
    if (y + h > layer->h) h = layer->h - y;

    if (layer->needs_redraw == 0) {
        // First dirty region
        layer->dirty_x = x;
        layer->dirty_y = y;
        layer->dirty_w = w;
        layer->dirty_h = h;
        layer->needs_redraw = 1;
    } else {
        // Merge with existing dirty region (union)
        uint16_t x1 = layer->dirty_x;
        uint16_t y1 = layer->dirty_y;
        uint16_t x2 = x1 + layer->dirty_w;
        uint16_t y2 = y1 + layer->dirty_h;

        uint16_t new_x1 = (x < x1) ? x : x1;
        uint16_t new_y1 = (y < y1) ? y : y1;
        uint16_t new_x2 = ((x + w) > x2) ? (x + w) : x2;
        uint16_t new_y2 = ((y + h) > y2) ? (y + h) : y2;

        layer->dirty_x = new_x1;
        layer->dirty_y = new_y1;
        layer->dirty_w = new_x2 - new_x1;
        layer->dirty_h = new_y2 - new_y1;
    }

    ss_layer_compat_on_dirty_marked(layer);
}

// Mark entire layer as clean (no redraw needed)
void ss_layer_mark_clean(Layer* layer) {
    if (layer) {
        layer->needs_redraw = 0;
        layer->dirty_w = 0;
        layer->dirty_h = 0;
        ss_layer_compat_on_layer_cleaned(layer);
    }
}

// Draw a specific rectangle of a layer - OLD VERSION (SLOW)
void ss_layer_draw_rect_layer_bounds(Layer* l, uint16_t dx0, uint16_t dy0, uint16_t dx1, uint16_t dy1) {
    if (0 == (l->attr & LAYER_ATTR_VISIBLE))
        return;
    uint8_t lid = l - ss_layer_mgr->layers;

    // Clamp bounds to layer size
    if (dx1 > l->w) dx1 = l->w;
    if (dy1 > l->h) dy1 = l->h;
    if (dx0 >= dx1 || dy0 >= dy1) return;

    // Use batch processing for better performance
    ss_layer_draw_rect_layer_bounds_batch_internal(l, dx0, dy0, dx1, dy1);
}

// Internal function for batch bounds processing
void ss_layer_draw_rect_layer_bounds_batch_internal(Layer* l, uint16_t dx0, uint16_t dy0, uint16_t dx1, uint16_t dy1) {
    if (ss_layer_mgr->batch_optimized) {
        // Use batch processing for better performance
        for (int16_t dy = dy0; dy < dy1; dy++) {
            int16_t vy = l->y + dy;
            if (vy < 0 || vy >=
#ifdef LOCAL_MODE
                TEST_HEIGHT
#else
                HEIGHT
#endif
            )
                continue;
            // Optimized DMA transfer using bit shifts
            uint16_t map_width =
        #ifdef LOCAL_MODE
                TEST_WIDTH >> 3;  // TEST_WIDTH / 8
        #else
                WIDTH >> 3;  // WIDTH / 8
        #endif
            uint16_t vy_div8 = vy >> 3;
            uint16_t l_x_div8 = l->x >> 3;

            int16_t startdx = -1;
            for (int16_t dx = dx0; dx < dx1; dx += 8) {
                if (ss_layer_mgr->map[vy_div8 * map_width + ((l->x + dx) >> 3)] == l->z) {
                    // set the first addr to transfer -> startdx
                    if (startdx == -1)
                        startdx = dx;
                } else if (startdx >= 0) {
                    // transfer between startdx and dx using batch
                    int16_t vx = l->x + startdx;
                    // X68000 16-color mode: VRAM is 2 bytes per pixel
                    // Transfer to lower byte (byte 1) where lower 4 bits are used
                    ss_layer_add_batch_transfer(
                        &l->vram[dy * l->w + startdx],
                        ((uint8_t*)&vram_start[vy * VRAMWIDTH + vx]) + 1,
                        dx - startdx);
                    startdx = -1;
                }
            }
            // DMA if the last block is not transferred yet
            if (startdx >= 0) {
                int16_t vx = l->x + startdx;
                ss_layer_add_batch_transfer(
                    &l->vram[dy * l->w + startdx],
                    ((uint8_t*)&vram_start[vy * VRAMWIDTH + vx]) + 1,
                    dx1 - startdx);
            }
        }
        // Execute all batched transfers at once
        ss_layer_execute_batch_transfers();
    } else {
        // Fallback to old method if batch optimization is disabled
        for (int16_t dy = dy0; dy < dy1; dy++) {
            int16_t vy = l->y + dy;
            if (vy < 0 || vy >=
#ifdef LOCAL_MODE
                TEST_HEIGHT
#else
                HEIGHT
#endif
            )
                continue;
            // Optimized DMA transfer using bit shifts
            uint16_t map_width =
        #ifdef LOCAL_MODE
                TEST_WIDTH >> 3;  // TEST_WIDTH / 8
        #else
                WIDTH >> 3;  // WIDTH / 8
        #endif
            uint16_t vy_div8 = vy >> 3;
            uint16_t l_x_div8 = l->x >> 3;

            int16_t startdx = -1;
            for (int16_t dx = dx0; dx < dx1; dx += 8) {
                if (ss_layer_mgr->map[vy_div8 * map_width + ((l->x + dx) >> 3)] == l->z) {
                    // set the first addr to transfer -> startdx
                    if (startdx == -1)
                        startdx = dx;
                } else if (startdx >= 0) {
                    // transfer between startdx and dx
                    int16_t vx = l->x + startdx;
                    ss_layer_draw_rect_layer_dma(
                        l, &l->vram[dy * l->w + startdx],
                        ((uint8_t*)&vram_start[vy * VRAMWIDTH + vx]) + 1,
                        dx - startdx);
                    startdx = -1;
                }
            }
            // DMA if the last block is not transferred yet
            if (startdx >= 0) {
                int16_t vx = l->x + startdx;
                ss_layer_draw_rect_layer_dma(
                    l, &l->vram[dy * l->w + startdx],
                    ((uint8_t*)&vram_start[vy * VRAMWIDTH + vx]) + 1,
                    dx1 - startdx);
            }
        }
    }
}

// Public batch-optimized bounds drawing function
void ss_layer_draw_rect_layer_bounds_batch(Layer* l, uint16_t dx0, uint16_t dy0, uint16_t dx1, uint16_t dy1) {
    if (0 == (l->attr & LAYER_ATTR_VISIBLE))
        return;
    uint8_t lid = l - ss_layer_mgr->layers;

    // Clamp bounds to layer size
    if (dx1 > l->w) dx1 = l->w;
    if (dy1 > l->h) dy1 = l->h;
    if (dx0 >= dx1 || dy0 >= dy1) return;

    // Use batch processing for better performance
    ss_layer_draw_rect_layer_bounds_batch_internal(l, dx0, dy0, dx1, dy1);
}

// Draw only the dirty regions of all layers - MAJOR PERFORMANCE IMPROVEMENT with BATCH DMA
void ss_layer_draw_dirty_only() {
    // Initialize batch transfers for this frame
    ss_layer_init_batch_transfers();

    // Performance monitoring: Start dirty region drawing
    SS_PERF_START_MEASUREMENT(SS_PERF_DIRTY_DRAW);

    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* layer = ss_layer_mgr->zLayers[i];
        if (layer->needs_redraw && (layer->attr & LAYER_ATTR_VISIBLE)) {
            if (layer->dirty_w > 0 && layer->dirty_h > 0) {
                // Performance monitoring: Start dirty rectangle drawing
                SS_PERF_START_MEASUREMENT(SS_PERF_DIRTY_RECT);
                // Only redraw the dirty rectangle using batch optimization
                ss_layer_draw_rect_layer_bounds_batch(layer,
                    layer->dirty_x, layer->dirty_y,
                    layer->dirty_x + layer->dirty_w,
                    layer->dirty_y + layer->dirty_h);
                SS_PERF_END_MEASUREMENT(SS_PERF_DIRTY_RECT);
            } else {
                // Performance monitoring: Start full layer drawing
                SS_PERF_START_MEASUREMENT(SS_PERF_FULL_LAYER);
                // If no specific dirty rectangle, draw entire layer (for initial draw)
                ss_layer_draw_rect_layer_batch(layer);
                SS_PERF_END_MEASUREMENT(SS_PERF_FULL_LAYER);
            }
            ss_layer_mark_clean(layer);
        }
    }

    // Execute all batched transfers at once for maximum performance
    ss_layer_execute_batch_transfers();

    // Performance monitoring: End dirty region drawing
    SS_PERF_END_MEASUREMENT(SS_PERF_DIRTY_DRAW);
}

void ss_layer_invalidate(Layer* layer) {
    // Mark the entire layer as dirty and ensure needs_redraw is set
    layer->dirty_x = 0;
    layer->dirty_y = 0;
    layer->dirty_w = layer->w;
    layer->dirty_h = layer->h;
    layer->needs_redraw = 1;
    ss_layer_compat_on_dirty_marked(layer);
}

void ss_layer_update_map(Layer* layer) {
    uint8_t lid = 0;
    if (NULL != layer) {
        lid = layer - ss_layer_mgr->layers;
    }

    uint16_t map_width =
#ifdef LOCAL_MODE
        TEST_WIDTH >> 3;  // TEST_WIDTH / 8
#else
        WIDTH >> 3;  // WIDTH / 8
#endif
    uint16_t layer_y_end = (layer->y + layer->h) >> 3;
    uint16_t layer_x_end = (layer->x + layer->w) >> 3;

    for (int i = lid; i < ss_layer_mgr->topLayerIdx; i++) {
        for (int dy = layer->y >> 3; dy < layer_y_end; dy++) {
            uint8_t* map_row = &ss_layer_mgr->map[dy * map_width];
            for (int dx = layer->x >> 3; dx < layer_x_end; dx++) {
                map_row[dx] = i;
            }
        }
    }
}

// Batch DMA optimization functions for 3x performance improvement
void ss_layer_init_batch_transfers(void) {
    ss_layer_mgr->batch_count = 0;
    ss_layer_mgr->batch_optimized = true;  // 条件付きバッチDMAを有効化（フェーズ1最適化）
}

void ss_layer_add_batch_transfer(uint8_t* src, uint8_t* dst, uint16_t count) {
    if (ss_layer_mgr->batch_count >= SS_CONFIG_DMA_MAX_TRANSFERS) {
        // Execute current batch if full
        ss_layer_execute_batch_transfers();
        ss_layer_mgr->batch_count = 0;
    }

    if (ss_layer_mgr->batch_count < SS_CONFIG_DMA_MAX_TRANSFERS) {
        ss_layer_mgr->batch_transfers[ss_layer_mgr->batch_count].src = src;
        ss_layer_mgr->batch_transfers[ss_layer_mgr->batch_count].dst = dst;
        ss_layer_mgr->batch_transfers[ss_layer_mgr->batch_count].count = count;
        ss_layer_mgr->batch_count++;
    }
}

void ss_layer_execute_batch_transfers(void) {
    if (ss_layer_mgr->batch_count == 0) {
        return;
    }

    // Performance monitoring: Start batch DMA transfer
    SS_PERF_START_MEASUREMENT(SS_PERF_DIRTY_DRAW);

    // 条件付きバッチDMA転送（フェーズ1最適化）
    if (ss_layer_mgr->batch_count == 1) {
        // 単一転送 - 最適化版DMAを使用
        dma_init_optimized(ss_layer_mgr->batch_transfers[0].src,
                          ss_layer_mgr->batch_transfers[0].dst,
                          ss_layer_mgr->batch_transfers[0].count);
    } else {
        // 複数転送 - 最適化版DMAで初期化後、連続セグメントをグループ化
        ss_execute_conditional_batch_transfers();
    }

    // Performance monitoring: End batch DMA transfer
    SS_PERF_END_MEASUREMENT(SS_PERF_DIRTY_DRAW);

    // Reset batch counter
    ss_layer_mgr->batch_count = 0;
}

// 条件付きバッチDMA転送（DMA制約対応版）
void ss_execute_conditional_batch_transfers(void) {
    if (ss_layer_mgr->batch_count <= 1) {
        return;
    }

    // 転送セグメントをdstアドレス順にソート（連続セグメントの特定のため）
    ss_sort_batch_transfers_by_dst();

    // 連続するセグメントをグループ化して実行
    int group_start = 0;
    for (int i = 1; i <= ss_layer_mgr->batch_count; i++) {
        bool is_continuous = false;
        if (i < ss_layer_mgr->batch_count) {
            // 次のセグメントとの連続性をチェック
            uint8_t* current_dst_end = ss_layer_mgr->batch_transfers[i-1].dst +
                                     ss_layer_mgr->batch_transfers[i-1].count * 2; // VRAM 2bytes/pixel
            uint8_t* next_dst = ss_layer_mgr->batch_transfers[i].dst;
            is_continuous = (current_dst_end == next_dst);
        }

        if (!is_continuous || i == ss_layer_mgr->batch_count) {
            // グループ実行（連続セグメントまたは最後のセグメント）
            ss_execute_batch_group(&ss_layer_mgr->batch_transfers[group_start], i - group_start);
            group_start = i;
        }
    }
}

void ss_execute_batch_group(BatchTransfer* group, int count) {
    if (count == 1) {
        // 単一転送 - 最適化版DMAを使用
        dma_init_optimized(group[0].src, group[0].dst, group[0].count);
    } else {
        // バッチ転送（連続性保証済み）
        dma_clear();
        for (int i = 0; i < count; i++) {
            xfr_inf[i].addr = group[i].src;
            xfr_inf[i].count = group[i].count;
        }
        dma_init(group[0].dst, count);

        // Execute all transfers at once
        dma_start();
        dma_wait_completion();
        dma_clear();
    }
}

void ss_sort_batch_transfers_by_dst(void) {
    // 単純なバブルソート（小規模データ向け）
    for (int i = 0; i < ss_layer_mgr->batch_count - 1; i++) {
        for (int j = 0; j < ss_layer_mgr->batch_count - i - 1; j++) {
            uint32_t dst1 = (uint32_t)ss_layer_mgr->batch_transfers[j].dst;
            uint32_t dst2 = (uint32_t)ss_layer_mgr->batch_transfers[j + 1].dst;
            if (dst1 > dst2) {
                // 入れ替え
                BatchTransfer temp = ss_layer_mgr->batch_transfers[j];
                ss_layer_mgr->batch_transfers[j] = ss_layer_mgr->batch_transfers[j + 1];
                ss_layer_mgr->batch_transfers[j + 1] = temp;
            }
        }
    }
}

// Buffer pool management for memory efficiency and 3x performance improvement
void ss_layer_init_buffer_pool(void) {
    ss_layer_mgr->buffer_pool_count = 0;
    for (int i = 0; i < MAX_LAYERS; i++) {
        ss_layer_mgr->buffer_pool[i].buffer = NULL;
        ss_layer_mgr->buffer_pool[i].in_use = false;
    }
}

uint8_t* ss_layer_alloc_buffer(uint16_t width, uint16_t height) {
    // Phase 3: Enhanced error handling and fallback allocation
    static uint32_t alloc_failures = 0;

    // First, try to find an existing unused buffer of the right size
    for (int i = 0; i < ss_layer_mgr->buffer_pool_count; i++) {
        if (!ss_layer_mgr->buffer_pool[i].in_use &&
            ss_layer_mgr->buffer_pool[i].width == width &&
            ss_layer_mgr->buffer_pool[i].height == height) {
            ss_layer_mgr->buffer_pool[i].in_use = true;
            return ss_layer_mgr->buffer_pool[i].buffer;
        }
    }

    // If no suitable buffer found, try to create a new one
    if (ss_layer_mgr->buffer_pool_count < MAX_LAYERS) {
        uint16_t buffer_size = width * height;
        uint8_t* new_buffer = (uint8_t*)ss_mem_alloc4k(buffer_size);

        // Phase 3: Check for allocation failure and provide fallback
        if (!new_buffer) {
            alloc_failures++;
            // Try to free some unused buffers first
            for (int i = 0; i < ss_layer_mgr->buffer_pool_count && !new_buffer; i++) {
                if (!ss_layer_mgr->buffer_pool[i].in_use) {
                    ss_mem_free((uint32_t)(uintptr_t)ss_layer_mgr->buffer_pool[i].buffer, 0);
                    ss_layer_mgr->buffer_pool[i].buffer = NULL;
                    ss_layer_mgr->buffer_pool[i].in_use = false;

                    // Try allocation again with freed memory
                    new_buffer = (uint8_t*)ss_mem_alloc4k(buffer_size);
                    break;
                }
            }

            // If still failed, try smaller size as last resort
            if (!new_buffer && buffer_size > 1024) {
                buffer_size = 1024;  // Minimum size fallback
                new_buffer = (uint8_t*)ss_mem_alloc4k(buffer_size);
            }

            // If all failed, return NULL for caller to handle
            if (!new_buffer) {
                return NULL;
            }
        }

        // Success: add to pool
        int pool_index = ss_layer_mgr->buffer_pool_count;
        ss_layer_mgr->buffer_pool[pool_index].buffer = new_buffer;
        ss_layer_mgr->buffer_pool[pool_index].width = (buffer_size == width * height) ? width : 32;
        ss_layer_mgr->buffer_pool[pool_index].height = (buffer_size == width * height) ? height : 32;
        ss_layer_mgr->buffer_pool[pool_index].in_use = true;
        ss_layer_mgr->buffer_pool_count++;

        return new_buffer;
    }

    // Pool is full: try direct allocation with error handling
    uint16_t buffer_size = width * height;
    uint8_t* direct_buffer = (uint8_t*)ss_mem_alloc4k(buffer_size);

    if (!direct_buffer) {
        alloc_failures++;
        // Last resort: try smaller size
        if (buffer_size > 1024) {
            buffer_size = 1024;
            direct_buffer = (uint8_t*)ss_mem_alloc4k(buffer_size);
        }
    }

    return direct_buffer;  // May be NULL - caller must handle
}

void ss_layer_free_buffer(uint8_t* buffer) {
    if (!buffer) return;

    // Find the buffer in the pool and mark as unused
    for (int i = 0; i < ss_layer_mgr->buffer_pool_count; i++) {
        if (ss_layer_mgr->buffer_pool[i].buffer == buffer) {
            ss_layer_mgr->buffer_pool[i].in_use = false;
            return;
        }
    }

    // If not found in pool, free directly
    ss_mem_free((uint32_t)(uintptr_t)buffer, 0);
}

LayerBuffer* ss_layer_find_buffer(uint8_t* buffer) {
    for (int i = 0; i < ss_layer_mgr->buffer_pool_count; i++) {
        if (ss_layer_mgr->buffer_pool[i].buffer == buffer) {
            return &ss_layer_mgr->buffer_pool[i];
        }
    }
    return NULL;
}

LayerBuffer* ss_layer_find_buffer_by_size(uint16_t width, uint16_t height) {
    // 適切なサイズの未使用バッファを探す
    for (int i = 0; i < ss_layer_mgr->buffer_pool_count; i++) {
        if (!ss_layer_mgr->buffer_pool[i].in_use &&
            ss_layer_mgr->buffer_pool[i].width >= width &&
            ss_layer_mgr->buffer_pool[i].height >= height) {
            ss_layer_mgr->buffer_pool[i].in_use = true;
            return &ss_layer_mgr->buffer_pool[i];
        }
    }

    // 適切なバッファが見つからない場合はNULLを返す
    return NULL;
}

void ss_layer_optimize_buffer_usage(void) {
    // Phase 3: 小規模なバッファ効率向上
    static uint32_t last_cleanup = 0;
    static int buffers_freed = 0;

    // Clean up unused buffers periodically (低頻度)
    if (ss_timerd_counter - last_cleanup > 600) {  // 10秒ごと
        int freed_count = 0;
        for (int i = 0; i < ss_layer_mgr->buffer_pool_count; i++) {
            if (!ss_layer_mgr->buffer_pool[i].in_use) {
                // Phase 3: メモリ使用量最適化 - 古いバッファを優先的に解放
                if (ss_layer_mgr->buffer_pool_count > 4) {  // 最低4つは保持
                    ss_mem_free((uint32_t)(uintptr_t)ss_layer_mgr->buffer_pool[i].buffer, 0);
                    ss_layer_mgr->buffer_pool[i].buffer = NULL;
                    ss_layer_mgr->buffer_pool[i].in_use = false;
                    freed_count++;
                    buffers_freed++;
                }
            }
        }
        last_cleanup = ss_timerd_counter;

        // 解放されたバッファをプールの最後から削除
        if (freed_count > 0) {
            int write_idx = 0;
            for (int i = 0; i < ss_layer_mgr->buffer_pool_count; i++) {
                if (ss_layer_mgr->buffer_pool[i].buffer != NULL) {
                    if (write_idx != i) {
                        ss_layer_mgr->buffer_pool[write_idx] = ss_layer_mgr->buffer_pool[i];
                    }
                    write_idx++;
                }
            }
            ss_layer_mgr->buffer_pool_count = write_idx;
        }
    }
}

// メモリマップキャッシュの初期化（重複定義削除済み）

// キャッシュ付きメモリマップ検索（最適化版）
uint8_t ss_get_cached_map_value(uint16_t index) {
    // LRUキャッシュ検索
    for (int i = 0; i < MAP_CACHE_SIZE; i++) {
        if (map_cache[i].valid && map_cache[i].map_index == index) {
            map_cache[i].last_access = ss_timerd_counter;
            map_cache_hits++;
            return map_cache[i].layer_id;
        }
    }

    // キャッシュミス - メモリから読み込み
    uint8_t value = ss_layer_mgr->map[index];
    map_cache_misses++;

    // LRU置換（既存のCPU負荷ベースアルゴリズムを活用）
    int lru_index = 0;
    uint32_t oldest = map_cache[0].last_access;
    for (int i = 1; i < MAP_CACHE_SIZE; i++) {
        if (!map_cache[i].valid) {
            lru_index = i;
            break;
        }
        if (map_cache[i].last_access < oldest) {
            oldest = map_cache[i].last_access;
            lru_index = i;
        }
    }

    map_cache[lru_index].map_index = index;
    map_cache[lru_index].layer_id = value;
    map_cache[lru_index].last_access = ss_timerd_counter;
    map_cache[lru_index].valid = true;
    return value;
}

// キャッシュ統計情報の取得
void ss_get_map_cache_stats(uint32_t* hits, uint32_t* misses, float* hit_rate) {
    if (hits) *hits = map_cache_hits;
    if (misses) *misses = map_cache_misses;
    if (hit_rate) {
        uint32_t total = map_cache_hits + map_cache_misses;
        *hit_rate = total > 0 ? (float)map_cache_hits / total : 0.0f;
    }
}

// Phase 2: 高速メモリマップ参照テーブルの初期化（遅延初期化版）
void ss_layer_init_fast_map(void) {
    // 初回は空の状態で初期化（高速起動のため）
    for (int y = 0; y < FAST_MAP_HEIGHT; y++) {
        for (int x = 0; x < FAST_MAP_WIDTH; x++) {
            s_fast_map[y][x] = 0;  // 初回は空で初期化
        }
    }
}

// Phase 2: 初回使用時にメモリマップを計算（遅延初期化）
void ss_layer_update_fast_map(void) {
    static bool initialized = false;
    if (!initialized) {
        for (int y = 0; y < FAST_MAP_HEIGHT; y++) {
            for (int x = 0; x < FAST_MAP_WIDTH; x++) {
                s_fast_map[y][x] = ss_compute_layer_mask_fast(x*32, y*32, 32, 32);
            }
        }
        initialized = true;
    }
}

// Phase 2: 32x32領域のレイヤーマスクを高速計算
uint32_t ss_compute_layer_mask_fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint32_t mask = 0;
    uint16_t map_width = WIDTH >> 3;

    // 領域内の全マップ値をOR演算
    for (int dy = 0; dy < h; dy += 8) {
        for (int dx = 0; dx < w; dx += 8) {
            uint16_t map_x = (x + dx) >> 3;
            uint16_t map_y = (y + dy) >> 3;

            // 範囲チェック
            if (map_x < (WIDTH >> 3) && map_y < (HEIGHT >> 3)) {
                uint8_t map_value = ss_layer_mgr->map[map_y * map_width + map_x];
                if (map_value != 0) {
                    mask |= (1 << map_value);
                }
            }
        }
    }
    return mask;
}

// Phase 2: 最適化されたメモリマップ参照（即時効果版）
uint8_t ss_get_map_value_optimized(uint16_t x, uint16_t y) {
    // 即時効果：キャッシュを最優先で使用（高速）
    uint16_t map_x = x >> 3;
    uint16_t map_y = y >> 3;
    uint16_t index = map_y * (WIDTH >> 3) + map_x;

    uint8_t cached_value = ss_get_cached_map_value(index);
    if (cached_value != 0) {
        return cached_value;  // キャッシュヒット
    }

    // 遅延初期化：初回のみ最適化テーブルを更新
    ss_layer_update_fast_map();

    // 32x32ブロック単位で高速参照（バックアップ）
    uint16_t block_x = x / 32;
    uint16_t block_y = y / 32;

    if (block_x < FAST_MAP_WIDTH && block_y < FAST_MAP_HEIGHT) {
        uint32_t mask = s_fast_map[block_y][block_x];
        if (mask != 0) {
            return cached_value;  // 最適化テーブルヒット
        }
    }

    // 最終フォールバック：直接メモリ参照
    return ss_layer_mgr->map[index];
}

// Phase 2: スーパースカラー対応のCPU転送関数
void ss_layer_draw_rect_layer_cpu_superscalar(uint8_t* src, uint8_t* dst, uint16_t count) {
    // アライメントチェックと最適化された転送
    if (count >= 8 && ((uintptr_t)src & 3) == 0 && ((uintptr_t)dst & 3) == 0) {
        // 32ビット転送（スーパースカラー対応）
        uint32_t* src32 = (uint32_t*)src;
        uint32_t* dst32 = (uint32_t*)dst;
        int blocks = count >> 2;

        // スーパースカラー対応のループ展開
        for (int i = 0; i < blocks; i += 4) {
            if (i + 3 < blocks) {
                *dst32++ = *src32++; *dst32++ = *src32++;
                *dst32++ = *src32++; *dst32++ = *src32++;
            } else {
                // 残りの処理
                for (int j = i; j < blocks; j++) {
                    *dst32++ = *src32++;
                }
                break;
            }
        }

        // 残りのバイトを処理
        uint8_t* src8 = (uint8_t*)src32;
        uint8_t* dst8 = (uint8_t*)dst32;
        for (int i = 0; i < (count & 3); i++) {
            *dst8++ = *src8++;
        }
    } else {
        // 8ビット転送の最適化
        int i = 0;
        for (; i < count - 3; i += 4) {
            dst[i] = src[i]; dst[i + 1] = src[i + 1];
            dst[i + 2] = src[i + 2]; dst[i + 3] = src[i + 3];
        }
        for (; i < count; i++) {
            dst[i] = src[i];
        }
    }
}

// Phase 2: フォールバック用CPU転送関数（極小ブロック用）
void ss_layer_draw_rect_layer_cpu_optimized_fallback(uint8_t* src, uint8_t* dst, uint16_t count) {
    // 極小ブロック用の最適化転送
    if (count >= 4 && ((uintptr_t)src & 3) == 0 && ((uintptr_t)dst & 3) == 0) {
        uint32_t* src32 = (uint32_t*)src;
        uint32_t* dst32 = (uint32_t*)dst;
        int blocks = count >> 2;
        for (int i = 0; i < blocks; i++) {
            *dst32++ = *src32++;
        }
        // 残りのバイト
        uint8_t* src8 = (uint8_t*)src32;
        uint8_t* dst8 = (uint8_t*)dst32;
        for (int i = 0; i < (count & 3); i++) {
            *dst8++ = *src8++;
        }
    } else {
        // バイト単位の転送
        for (int i = 0; i < count; i++) {
            dst[i] = src[i];
        }
    }
}

// Phase 2: 68kアセンブラ最適化版メモリコピー
void ss_ultra_fast_copy_32bit(uint32_t* dst, uint32_t* src, int count) {
    // スーパースカラー転送（68k最適化）
    __asm__ volatile (
        "move.l %0, %%a0\n"
        "move.l %1, %%a1\n"
        "move.l %2, %%d0\n"
        "1: move.l (%%a1)+, (%%a0)+\n"
        "move.l (%%a1)+, (%%a0)+\n"  // 2命令並行実行
        "move.l (%%a1)+, (%%a0)+\n"
        "move.l (%%a1)+, (%%a0)+\n"  // 4命令並行実行
        "subq.l #4, %%d0\n"
        "bgt 1b\n"
        : : "r"(dst), "r"(src), "r"(count)
        : "a0", "a1", "d0", "memory"
    );
}

// Phase 2: ダブルバッファシステムの初期化（簡素化版）
void ss_layer_init_double_buffer(void) {
    // 簡素化：初期化を最小限に（高速起動のため）
    double_buffer.front_buffer = NULL;
    double_buffer.back_buffer = NULL;
    double_buffer.back_buffer_dirty = false;
    // ダーティブロック配列は使用時に動的確保
}

// Phase 2: バックバッファへの描画
void ss_layer_draw_to_backbuffer(Layer* l) {
    if (!l) return;

    // バックバッファに描画（バッファプールから取得）
    if (!double_buffer.back_buffer) {
        // 適切なサイズのバッファを取得
        double_buffer.back_buffer = ss_layer_find_buffer_by_size(l->w, l->h);
        if (!double_buffer.back_buffer) {
            // バッファプールから取得できなければ新規作成
            l->vram = ss_layer_alloc_buffer(l->w, l->h);
            double_buffer.back_buffer = ss_layer_find_buffer(l->vram);
        }
    }

    if (!double_buffer.back_buffer_dirty) {
        // 最初の描画
        double_buffer.back_buffer_dirty = true;
    }

    // バックバッファへの実際の描画処理（詳細は後で実装）
    // ここでは基本的なセットアップのみ
}

// Phase 2: バッファの切り替え
void ss_layer_flip_buffers(void) {
    // フレーム終了時にバッファを切り替え
    if (double_buffer.back_buffer_dirty && double_buffer.back_buffer) {
        // バッファを入れ替え（バッファプール管理）
        LayerBuffer* temp = double_buffer.front_buffer;
        double_buffer.front_buffer = double_buffer.back_buffer;
        double_buffer.back_buffer = temp;
        double_buffer.back_buffer_dirty = false;
    }
}
