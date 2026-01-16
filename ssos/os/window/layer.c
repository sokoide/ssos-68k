#include "layer.h"
#include "dma.h"
#include "damage.h"
#include "kernel.h"
#include "memory.h"
#include "vram.h"
#include "ss_perf.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void ss_layer_cpu_copy(uint8_t* dst, uint8_t* src, uint16_t count) {
    if (count >= 4 && ((uintptr_t)src & 3) == 0 && ((uintptr_t)dst & 3) == 0) {
        uint32_t* src32 = (uint32_t*)src;
        uint32_t* dst32 = (uint32_t*)dst;
        uint16_t blocks = count >> 2;
        for (uint16_t i = 0; i < blocks; i++) {
            *dst32++ = *src32++;
        }
        uint8_t* src8 = (uint8_t*)src32;
        uint8_t* dst8 = (uint8_t*)dst32;
        for (uint16_t i = 0; i < (count & 3); i++) {
            *dst8++ = *src8++;
        }
    } else {
        for (uint16_t i = 0; i < count; i++) {
            dst[i] = src[i];
        }
    }
    g_damage_perf.cpu_transfers_count++;
}

static void ss_layer_dma_copy(uint8_t* dst, uint8_t* src, uint16_t count) {
    dma_prepare_x68k_16color();
    dma_clear();
    dma_setup_span(dst, src, count);
    dma_start();
    dma_wait_completion();
    dma_clear();
    g_damage_perf.dma_transfers_count++;
}

LayerMgr* ss_layer_mgr;

void ss_layer_init() {
    ss_layer_mgr = (LayerMgr*)ss_mem_alloc4k(sizeof(LayerMgr));
    ss_layer_mgr->topLayerIdx = 0;
    for (int i = 0; i < MAX_LAYERS; i++) {
        ss_layer_mgr->layers[i].attr = 0;
    }
    uint32_t map_bytes = (WIDTH >> 3) * (HEIGHT >> 3);
    ss_layer_mgr->map = (uint8_t*)ss_mem_alloc4k(map_bytes);
    memset(ss_layer_mgr->map, 0, map_bytes);
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

    ss_layer_rebuild_z_map();
}

void ss_all_layer_draw() {
    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* layer = ss_layer_mgr->zLayers[i];
        ss_layer_draw_rect_layer_bounds(layer, 0, 0, WIDTH, HEIGHT);
    }
}

static void ss_layer_fill_map_for_layer(const Layer* layer) {
    if (!layer || !(layer->attr & LAYER_ATTR_VISIBLE)) {
        return;
    }

    uint16_t blocks_w = (layer->w + 7) >> 3;
    uint16_t blocks_h = (layer->h + 7) >> 3;
    uint16_t map_width = WIDTH >> 3;
    uint16_t map_height = HEIGHT >> 3;
    uint16_t start_x = layer->x >> 3;
    uint16_t start_y = layer->y >> 3;

    for (uint16_t by = 0; by < blocks_h; by++) {
        uint16_t map_y = start_y + by;
        if (map_y >= map_height) {
            break;
        }
        uint8_t* row = &ss_layer_mgr->map[map_y * map_width];
        for (uint16_t bx = 0; bx < blocks_w; bx++) {
            uint16_t map_x = start_x + bx;
            if (map_x >= map_width) {
                break;
            }
            row[map_x] = layer->z;
        }
    }
}

void ss_layer_rebuild_z_map(void) {
    if (!ss_layer_mgr || !ss_layer_mgr->map) {
        return;
    }

    uint32_t map_bytes = (WIDTH >> 3) * (HEIGHT >> 3);
    memset(ss_layer_mgr->map, 0, map_bytes);

    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        ss_layer_fill_map_for_layer(ss_layer_mgr->zLayers[i]);
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

// Fast visibility probe using the 8x8 ownership map.
bool ss_layer_region_visible(const Layer* layer, uint16_t local_x, uint16_t local_y,
                             uint16_t width, uint16_t height) {
    if (!layer || !(layer->attr & LAYER_ATTR_USED) || !(layer->attr & LAYER_ATTR_VISIBLE)) {
        return false;
    }
    if (g_damage_perf.total_regions_processed < 10) {
        // Allow early frames to draw without occlusion pruning to avoid startup artifacts
        return true;
    }
    if (layer->z == 0) {
        // Always render the background layer to avoid missed redraws
        return true;
    }
    if (!ss_layer_mgr || !ss_layer_mgr->map || width == 0 || height == 0) {
        return true;
    }

    uint32_t global_x0 = (uint32_t)layer->x + (uint32_t)local_x;
    uint32_t global_y0 = (uint32_t)layer->y + (uint32_t)local_y;
    uint32_t global_x1 = global_x0 + (uint32_t)width;
    uint32_t global_y1 = global_y0 + (uint32_t)height;

    if (global_x0 >= WIDTH || global_y0 >= HEIGHT) {
        return true;
    }
    if (global_x1 > WIDTH) global_x1 = WIDTH;
    if (global_y1 > HEIGHT) global_y1 = HEIGHT;

    uint16_t map_width = WIDTH >> 3;
    uint16_t map_height = HEIGHT >> 3;

    uint16_t block_x0 = global_x0 >> 3;
    uint16_t block_y0 = global_y0 >> 3;
    uint16_t block_x1 = (global_x1 + 7) >> 3; // exclusive upper bound
    uint16_t block_y1 = (global_y1 + 7) >> 3;

    if (block_x0 >= map_width || block_y0 >= map_height) {
        return true;
    }
    if (block_x1 > map_width) block_x1 = map_width;
    if (block_y1 > map_height) block_y1 = map_height;
    if (block_x1 <= block_x0 || block_y1 <= block_y0) {
        return true;
    }

    uint8_t layer_z = layer->z;
    for (uint16_t by = block_y0; by < block_y1; ++by) {
        uint32_t row_base = (uint32_t)by * map_width;
        for (uint16_t bx = block_x0; bx < block_x1; ++bx) {
            if (ss_layer_mgr->map[row_base + bx] == layer_z) {
                return true;
            }
        }
    }

    return false;
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
// Optimized scanline-based dirty region drawing with interval merging
// Hybrid approach: 8-pixel blocks with optimized merging for better balance
void ss_layer_draw_rect_layer_bounds(Layer* l, uint16_t dx0, uint16_t dy0, uint16_t dx1, uint16_t dy1) {
    if (0 == (l->attr & LAYER_ATTR_VISIBLE))
        return;

    // Clamp bounds to layer size
    if (dx1 > l->w) dx1 = l->w;
    if (dy1 > l->h) dy1 = l->h;
    if (dx0 >= dx1 || dy0 >= dy1) return;

    // Optimize: Use 8-pixel alignment for faster processing but with improved merging
    uint16_t aligned_dx0 = dx0 & ~0x7;  // Round down to 8-pixel boundary
    uint16_t aligned_dx1 = (dx1 + 7) & ~0x7;  // Round up to 8-pixel boundary

    for (int16_t dy = dy0; dy < dy1; dy++) {
        int16_t vy = l->y + dy;
        if (vy < 0 || vy >= HEIGHT)
            continue;
        ss_update_performance_metrics();

        // Pre-calculate values for this scanline
        uint16_t map_width = WIDTH >> 3;  // WIDTH / 8
        uint16_t vy_div8 = vy >> 3;
        uint16_t l_x_div8 = l->x >> 3;
        uint8_t layer_z = l->z;

        int16_t startdx = -1;
        uint16_t consecutive_count = 0;

        // Process in 8-pixel blocks for efficiency, but merge consecutive blocks
        for (int16_t dx = aligned_dx0; dx < aligned_dx1; dx += 8) {
            uint16_t vx = l->x + dx;
            if (vx >= WIDTH) break;

            // Check 8-pixel block visibility (faster than per-pixel check)
            if (ss_layer_mgr->map[vy_div8 * map_width + (vx >> 3)] == layer_z) {
                if (startdx == -1) {
                    // Start new merged region
                    startdx = dx;
                    consecutive_count = 8;
                } else {
                    // Extend existing region (no gap)
                    consecutive_count += 8;
                }
            } else if (startdx >= 0) {
                // End of visible region - perform DMA transfer for merged blocks
                uint16_t transfer_x = l->x + startdx;
                uint16_t actual_start = (startdx < dx0) ? dx0 : startdx;
                uint16_t actual_end = ((startdx + consecutive_count) > dx1) ? dx1 : (startdx + consecutive_count);
                uint16_t transfer_width = actual_end - actual_start;

                if (transfer_width > 0) {
                    uint8_t* src = &l->vram[dy * l->w + actual_start];
                    uint8_t* dst = ((uint8_t*)&vram_start[vy * VRAMWIDTH + transfer_x]) + 1 + (actual_start - startdx);
                    if (transfer_width <= ss_adaptive_dma_threshold) {
                        ss_layer_cpu_copy(dst, src, transfer_width);
                    } else {
                        ss_layer_dma_copy(dst, src, transfer_width);
                    }
                }

                startdx = -1;
                consecutive_count = 0;
            }
        }

        // Handle final merged region if it extends to the end
        if (startdx >= 0) {
            uint16_t transfer_x = l->x + startdx;
            uint16_t actual_start = (startdx < dx0) ? dx0 : startdx;
            uint16_t actual_end = ((startdx + consecutive_count) > dx1) ? dx1 : (startdx + consecutive_count);
            uint16_t transfer_width = actual_end - actual_start;

            if (transfer_width > 0) {
                uint8_t* src = &l->vram[dy * l->w + actual_start];
                uint8_t* dst = ((uint8_t*)&vram_start[vy * VRAMWIDTH + transfer_x]) + 1 + (actual_start - startdx);
                if (transfer_width <= ss_adaptive_dma_threshold) {
                    ss_layer_cpu_copy(dst, src, transfer_width);
                } else {
                    ss_layer_dma_copy(dst, src, transfer_width);
                }
            }
        }
    }
}

// Draw only the dirty regions of all layers - MAJOR PERFORMANCE IMPROVEMENT
// Optimized dirty region drawing with scanline-based interval merging
void ss_layer_draw_dirty_only() {
    // Performance monitoring: Start dirty region drawing
    SS_PERF_START_MEASUREMENT(SS_PERF_DIRTY_DRAW);

    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* layer = ss_layer_mgr->zLayers[i];
        if (layer->needs_redraw && (layer->attr & LAYER_ATTR_VISIBLE)) {
            if (layer->dirty_w > 0 && layer->dirty_h > 0) {
                // Performance monitoring: Start dirty rectangle drawing
                SS_PERF_START_MEASUREMENT(SS_PERF_DIRTY_RECT);

                // Draw the dirty rectangle using the new optimized scanline-based method
                ss_layer_draw_rect_layer_bounds(layer,
                    layer->dirty_x, layer->dirty_y,
                    layer->dirty_x + layer->dirty_w,
                    layer->dirty_y + layer->dirty_h);

                SS_PERF_END_MEASUREMENT(SS_PERF_DIRTY_RECT);
            } else {
                // Performance monitoring: Start full layer drawing
                SS_PERF_START_MEASUREMENT(SS_PERF_FULL_LAYER);

                // For initial draw or full layer invalidation, use optimized method too
                ss_layer_draw_rect_layer_bounds(layer, 0, 0, layer->w, layer->h);

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

// Find the topmost visible layer at the given screen position
Layer* ss_layer_find_at_position(uint16_t x, uint16_t y) {
    // Check layers from top to bottom (highest z first)
    for (int i = ss_layer_mgr->topLayerIdx - 1; i >= 0; i--) {
        Layer* layer = ss_layer_mgr->zLayers[i];

        // Check if layer is visible and within bounds
        if ((layer->attr & LAYER_ATTR_VISIBLE) &&
            x >= layer->x && x < layer->x + layer->w &&
            y >= layer->y && y < layer->y + layer->h) {
            return layer;
        }
    }
    return NULL; // No layer found at this position
}

// Bring a layer to the front (highest z-order)
void ss_layer_bring_to_front(Layer* layer) {
    if (!layer || !(layer->attr & LAYER_ATTR_USED)) {
        return;
    }

    // Calculate layer id to check if it's the background layer
    uint8_t layer_id = layer - ss_layer_mgr->layers;

    // Background layer (layer id 0) should always stay at the bottom
    if (layer_id == 0) {
        return; // Don't move the background layer
    }

    // Find current position of this layer in z-order
    int current_pos = -1;
    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        if (ss_layer_mgr->zLayers[i] == layer) {
            current_pos = i;
            break;
        }
    }

    // If layer is already on top, nothing to do
    if (current_pos == ss_layer_mgr->topLayerIdx - 1) {
        return;
    }

    // Remove layer from current position
    if (current_pos >= 0) {
        // Shift layers down to fill the gap
        for (int i = current_pos; i < ss_layer_mgr->topLayerIdx - 1; i++) {
            ss_layer_mgr->zLayers[i] = ss_layer_mgr->zLayers[i + 1];
            // Update z value
            ss_layer_mgr->zLayers[i]->z = i;
        }
    }

    // Place layer at the top
    ss_layer_mgr->zLayers[ss_layer_mgr->topLayerIdx - 1] = layer;
    layer->z = ss_layer_mgr->topLayerIdx - 1;

    // Update z-order map to reflect the new layer order
    ss_layer_rebuild_z_map();

    // Mark the affected area as dirty for redraw
    ss_layer_mark_dirty(layer, 0, 0, layer->w, layer->h);
}

// Set layer to a specific z-order position
void ss_layer_set_z_order(Layer* layer, uint16_t new_z) {
    if (!layer || !(layer->attr & LAYER_ATTR_USED) || new_z >= ss_layer_mgr->topLayerIdx) {
        return;
    }

    uint16_t old_z = layer->z;
    if (old_z == new_z) {
        return; // No change needed
    }

    // Remove layer from current position
    for (int i = old_z; i < ss_layer_mgr->topLayerIdx - 1; i++) {
        ss_layer_mgr->zLayers[i] = ss_layer_mgr->zLayers[i + 1];
        ss_layer_mgr->zLayers[i]->z = i;
    }

    // レイヤーを新しい位置に挿入するためにシフト
    for (int i = ss_layer_mgr->topLayerIdx - 1; i > new_z; i--) {
        ss_layer_mgr->zLayers[i] = ss_layer_mgr->zLayers[i - 1];
        ss_layer_mgr->zLayers[i]->z = i;
    }

    // Place layer at new position
    ss_layer_mgr->zLayers[new_z] = layer;
    layer->z = new_z;

    ss_layer_rebuild_z_map();

    // Mark affected areas as dirty for redraw
    ss_layer_mark_dirty(layer, 0, 0, layer->w, layer->h);
}
