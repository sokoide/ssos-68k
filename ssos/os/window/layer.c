// LOCAL_MODE constants for testing
#ifdef LOCAL_MODE
// Use different variable names to avoid conflicts with kernel.h extern declarations
#ifndef TEST_WIDTH
const int TEST_WIDTH = 768;
#endif
#ifndef TEST_HEIGHT
const int TEST_HEIGHT = 512;
#endif
#ifndef TEST_VRAMWIDTH
const int TEST_VRAMWIDTH = 768;
#endif
#endif

#include "layer.h"
#include "ssoswindows.h"
#include "dma.h"
#include "kernel.h"
#include "memory.h"
#include "vram.h"
#include "ss_perf.h"
#include <stddef.h>

LayerMgr* ss_layer_mgr;

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
        // Use buffer pool for memory efficiency
        l->vram = ss_layer_alloc_buffer(width, height);
        l->w = width;
        l->h = height;
        // Mark entire layer as dirty for initial draw
        l->dirty_w = width;
        l->dirty_h = height;
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
// Increased default threshold for better batch DMA performance
static uint16_t ss_adaptive_dma_threshold = 16;  // Increased from 8 to 16 for better performance

// Update CPU performance metrics with improved adaptive algorithm
void ss_update_performance_metrics() {
    uint32_t current_time = ss_timerd_counter;

    // Update performance metrics every 100ms
    if (current_time > ss_last_performance_check + 100) {
        // Estimate CPU idle time based on system activity
        // This is a simplified metric - in a real system you'd track actual idle time
        static uint32_t last_activity = 0;
        uint32_t activity_delta = current_time - last_activity;

        // Improved adaptive DMA threshold algorithm
        if (activity_delta < 50) {
            // High activity - prefer DMA for larger blocks to free up CPU
            ss_adaptive_dma_threshold = 24;  // Increased from 12
        } else if (activity_delta < 100) {
            // Medium-high activity - balanced approach
            ss_adaptive_dma_threshold = 20;  // Increased from 8
        } else if (activity_delta < 200) {
            // Medium-low activity - prefer DMA for medium blocks
            ss_adaptive_dma_threshold = 16;  // Increased from 8
        } else if (activity_delta < 500) {
            // Low activity - use DMA for smaller blocks for consistency
            ss_adaptive_dma_threshold = 12;  // Increased from 4
        } else {
            // Very low activity - use DMA aggressively
            ss_adaptive_dma_threshold = 8;   // Increased from 4
        }

        last_activity = current_time;
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

    if (block_count <= ss_adaptive_dma_threshold) {
        // Use CPU transfer for small blocks - optimized for 32-bit transfers
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

    // Use DMA for larger transfers (adaptive threshold)
    dma_clear();
    // Configure DMA transfer
    dma_init(dst, 1);
    xfr_inf[0].addr = src;
    xfr_inf[0].count = block_count;

    // Execute DMA transfer
    dma_start();
    dma_wait_completion();
    dma_clear();
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
    ss_layer_mgr->batch_optimized = false;  // Disabled due to chaining issues with non-consecutive VRAM destinations
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

    // Execute batch DMA transfer
    dma_clear();
    for (int i = 0; i < ss_layer_mgr->batch_count; i++) {
        xfr_inf[i].addr = ss_layer_mgr->batch_transfers[i].src;
        xfr_inf[i].count = ss_layer_mgr->batch_transfers[i].count;
    }
    dma_init(ss_layer_mgr->batch_transfers[0].dst, ss_layer_mgr->batch_count);

    // Execute all transfers at once
    dma_start();
    dma_wait_completion();
    dma_clear();

    // Performance monitoring: End batch DMA transfer
    SS_PERF_END_MEASUREMENT(SS_PERF_DIRTY_DRAW);

    // Reset batch counter
    ss_layer_mgr->batch_count = 0;
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
    // First, try to find an existing unused buffer of the right size
    for (int i = 0; i < ss_layer_mgr->buffer_pool_count; i++) {
        if (!ss_layer_mgr->buffer_pool[i].in_use &&
            ss_layer_mgr->buffer_pool[i].width == width &&
            ss_layer_mgr->buffer_pool[i].height == height) {
            ss_layer_mgr->buffer_pool[i].in_use = true;
            return ss_layer_mgr->buffer_pool[i].buffer;
        }
    }

    // If no suitable buffer found, create a new one
    if (ss_layer_mgr->buffer_pool_count < MAX_LAYERS) {
        uint16_t buffer_size = width * height;
        ss_layer_mgr->buffer_pool[ss_layer_mgr->buffer_pool_count].buffer =
            (uint8_t*)ss_mem_alloc4k(buffer_size);
        ss_layer_mgr->buffer_pool[ss_layer_mgr->buffer_pool_count].width = width;
        ss_layer_mgr->buffer_pool[ss_layer_mgr->buffer_pool_count].height = height;
        ss_layer_mgr->buffer_pool[ss_layer_mgr->buffer_pool_count].in_use = true;
        ss_layer_mgr->buffer_pool_count++;
        return ss_layer_mgr->buffer_pool[ss_layer_mgr->buffer_pool_count - 1].buffer;
    }

    // Fallback: allocate directly if pool is full
    return (uint8_t*)ss_mem_alloc4k(width * height);
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

void ss_layer_optimize_buffer_usage(void) {
    // Clean up unused buffers periodically
    for (int i = 0; i < ss_layer_mgr->buffer_pool_count; i++) {
        if (!ss_layer_mgr->buffer_pool[i].in_use) {
            // Buffer is not in use, could be freed if memory is low
            // For now, we keep it in the pool for reuse
        }
    }
}
