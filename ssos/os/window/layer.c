#include "layer.h"
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
    ss_layer_mgr->map = (uint8_t*)ss_mem_alloc4k(WIDTH / 8 * HEIGHT / 8);
    // reset to 0 every 4 bytes
    uint32_t* p = (uint32_t*)ss_layer_mgr->map;
    for (int i = 0; i < WIDTH / 8 * HEIGHT / 8 / sizeof(uint32_t); i++) {
        *p++ = 0;
    }
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

void ss_layer_set(Layer* layer, uint8_t* vram, uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h) {
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
    uint16_t map_width = WIDTH >> 3;  // WIDTH / 8
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


void ss_all_layer_draw() { ss_all_layer_draw_rect(0, 0, WIDTH, HEIGHT); }

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
// in vram coordinates for the layer id
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
    for (int16_t dy = dy0; dy < dy1; dy++) {
        int16_t vy = l->y + dy;
        if (vy < 0 || vy >= HEIGHT)
            continue;
        // Optimized DMA transfer using bit shifts
        uint16_t map_width = WIDTH >> 3;  // WIDTH / 8
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

// Performance monitoring variables for adaptive DMA thresholds
static uint32_t ss_cpu_idle_time = 0;
static uint32_t ss_last_performance_check = 0;
static uint16_t ss_adaptive_dma_threshold = 8;

// Update CPU performance metrics
void ss_update_performance_metrics() {
    uint32_t current_time = ss_timerd_counter;

    // Update performance metrics every 100ms
    if (current_time > ss_last_performance_check + 100) {
        // Estimate CPU idle time based on system activity
        // This is a simplified metric - in a real system you'd track actual idle time
        static uint32_t last_activity = 0;
        uint32_t activity_delta = current_time - last_activity;

        // Adjust DMA threshold based on recent activity
        if (activity_delta < 50) {
            // High activity - prefer DMA for larger blocks
            ss_adaptive_dma_threshold = 12;
        } else if (activity_delta > 200) {
            // Low activity - use DMA even for smaller blocks
            ss_adaptive_dma_threshold = 4;
        } else {
            // Normal activity - use balanced threshold
            ss_adaptive_dma_threshold = 8;
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

    // OPTIMIZED: Use X68000 16-color mode specific DMA for better performance
    if (block_count <= ss_adaptive_dma_threshold) {
        // Use CPU transfer for small blocks
        if (block_count >= 4 && ((uintptr_t)src & 3) == 0 && ((uintptr_t)dst & 3) == 0) {
            // 32-bit aligned transfer for optimal CPU performance
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
            // Byte-by-byte transfer for unaligned data
            for (int i = 0; i < block_count; i++) {
                dst[i] = src[i];
            }
        }
        return;
    }

    // Use optimized X68000 16-color mode DMA for larger transfers
        dma_clear();
        dma_init_x68k_16color(dst, src, block_count);
        dma_start();
        dma_wait_completion();
        dma_clear();
}



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
}

// Mark entire layer as clean (no redraw needed)
void ss_layer_mark_clean(Layer* layer) {
    if (layer) {
        layer->needs_redraw = 0;
        layer->dirty_w = 0;
        layer->dirty_h = 0;
    }
}

// Draw a specific rectangle of a layer
void ss_layer_draw_rect_layer_bounds(Layer* l, uint16_t dx0, uint16_t dy0, uint16_t dx1, uint16_t dy1) {
    if (0 == (l->attr & LAYER_ATTR_VISIBLE))
        return;
    uint8_t lid = l - ss_layer_mgr->layers;

    // Clamp bounds to layer size
    if (dx1 > l->w) dx1 = l->w;
    if (dy1 > l->h) dy1 = l->h;
    if (dx0 >= dx1 || dy0 >= dy1) return;

    for (int16_t dy = dy0; dy < dy1; dy++) {
        int16_t vy = l->y + dy;
        if (vy < 0 || vy >= HEIGHT)
            continue;
        // Optimized DMA transfer using bit shifts
        uint16_t map_width = WIDTH >> 3;  // WIDTH / 8
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

// Draw only the dirty regions of all layers - MAJOR PERFORMANCE IMPROVEMENT
void ss_layer_draw_dirty_only() {
    // Performance monitoring: Start dirty region drawing
    SS_PERF_START_MEASUREMENT(SS_PERF_DIRTY_DRAW);

    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* layer = ss_layer_mgr->zLayers[i];
        if (layer->needs_redraw && (layer->attr & LAYER_ATTR_VISIBLE)) {
            if (layer->dirty_w > 0 && layer->dirty_h > 0) {
                // Performance monitoring: Start dirty rectangle drawing
                SS_PERF_START_MEASUREMENT(SS_PERF_DIRTY_RECT);
                // Only redraw the dirty rectangle
                ss_layer_draw_rect_layer_bounds(layer,
                    layer->dirty_x, layer->dirty_y,
                    layer->dirty_x + layer->dirty_w,
                    layer->dirty_y + layer->dirty_h);
                SS_PERF_END_MEASUREMENT(SS_PERF_DIRTY_RECT);
            } else {
                // Performance monitoring: Start full layer drawing
                SS_PERF_START_MEASUREMENT(SS_PERF_FULL_LAYER);
                // If no specific dirty rectangle, draw entire layer (for initial draw)
                ss_layer_draw_rect_layer(layer);
                SS_PERF_END_MEASUREMENT(SS_PERF_FULL_LAYER);
            }
            ss_layer_mark_clean(layer);
        }
    }

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
}

