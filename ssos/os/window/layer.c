#include "layer.h"
#include "damage.h"
#include "dma.h"
#include "kernel.h"
#include "memory.h"
#include "ss_perf.h"
#include "vram.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void ss_layer_cpu_copy(uint8_t* dst, uint8_t* src, uint16_t count) {
    // 16-byte unrolled loop for aligned transfers (4x32-bit per iteration)
    if (count >= 16 && ((uintptr_t)src & 3) == 0 && ((uintptr_t)dst & 3) == 0) {
        uint32_t* src32 = (uint32_t*)src;
        uint32_t* dst32 = (uint32_t*)dst;
        uint16_t blocks16 = count >> 4; // count / 16

        // Unrolled loop: 4 x 32-bit = 16 bytes per iteration
        for (uint16_t i = 0; i < blocks16; i++) {
            *dst32++ = *src32++;
            *dst32++ = *src32++;
            *dst32++ = *src32++;
            *dst32++ = *src32++;
        }

        // Handle remaining 4-15 bytes in 4-byte chunks
        uint16_t remaining = count & 15; // count % 16
        uint16_t blocks4 = remaining >> 2;
        for (uint16_t i = 0; i < blocks4; i++) {
            *dst32++ = *src32++;
        }

        // Handle final 0-3 bytes
        uint8_t* src8 = (uint8_t*)src32;
        uint8_t* dst8 = (uint8_t*)dst32;
        for (uint16_t i = 0; i < (remaining & 3); i++) {
            *dst8++ = *src8++;
        }
    } else if (count >= 4 && ((uintptr_t)src & 3) == 0 &&
               ((uintptr_t)dst & 3) == 0) {
        // 4-byte aligned but small (4-15 bytes)
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
        // Unaligned fallback
        for (uint16_t i = 0; i < count; i++) {
            dst[i] = src[i];
        }
    }
}

// VRAM blit: transfers pixels from offscreen buffer to X68000 VRAM
// X68000 16-color mode: each pixel occupies one word (2 bytes) in VRAM,
// with the pixel value stored in the low byte (+1 offset / odd address).
// Pixels are NOT contiguous in VRAM - each pixel is 2 bytes apart.
static void ss_layer_blit_to_vram(volatile uint16_t* vram, uint16_t vram_offset,
                                  const uint8_t* src, uint16_t count) {
    volatile uint16_t* dst = vram + vram_offset;
    for (uint16_t i = 0; i < count; i++) {
        dst[i] = src[i];
    }
    g_damage_perf.cpu_transfers_count++;
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

            // Initialize dirty rectangle tracking - mark entire layer as dirty
            // initially
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

// Fast visibility probe - B1 Optimization
// For few layers (≤ 8), Z-map scan overhead is worse than just drawing.
// Always return true and let the scanline-level 8x8 block check handle it.
// The Z-map is still used per-scanline in ss_layer_draw_rect_layer_bounds
// for efficient block merging, but the outer-level visibility check is
// simplified to avoid double-scanning.
bool ss_layer_region_visible(const Layer* layer, uint16_t local_x,
                             uint16_t local_y, uint16_t width,
                             uint16_t height) {
    if (!layer || !(layer->attr & LAYER_ATTR_USED) ||
        !(layer->attr & LAYER_ATTR_VISIBLE)) {
        return false;
    }
    // B1 OPTIMIZATION: With few layers, always render and let
    // the scanline-level Z-map check handle occlusion per-block.
    // This avoids the costly nested loop that scans the Z-map twice.
    (void)local_x;
    (void)local_y;
    (void)width;
    (void)height;
    return true;
}

// Mark a rectangular region of a layer as dirty (needs redrawing)
void ss_layer_mark_dirty(Layer* layer, uint16_t x, uint16_t y, uint16_t w,
                         uint16_t h) {
    if (!layer || w == 0 || h == 0)
        return;

    // Clamp to layer bounds
    if (x >= layer->w || y >= layer->h)
        return;
    if (x + w > layer->w)
        w = layer->w - x;
    if (y + h > layer->h)
        h = layer->h - y;

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
void ss_layer_draw_rect_layer_bounds(Layer* l, uint16_t dx0, uint16_t dy0,
                                     uint16_t dx1, uint16_t dy1) {
    if (0 == (l->attr & LAYER_ATTR_VISIBLE))
        return;

    // Skip transfer for DIRECT layers - they render directly to VRAM
    if (l->attr & LAYER_ATTR_DIRECT)
        return;

    // Clamp bounds to layer size
    if (dx1 > l->w)
        dx1 = l->w;
    if (dy1 > l->h)
        dy1 = l->h;
    if (dx0 >= dx1 || dy0 >= dy1)
        return;

    // Optimize: Use 8-pixel alignment for faster processing but with improved
    // merging
    uint16_t aligned_dx0 = dx0 & ~0x7;       // Round down to 8-pixel boundary
    uint16_t aligned_dx1 = (dx1 + 7) & ~0x7; // Round up to 8-pixel boundary

    for (int16_t dy = dy0; dy < dy1; dy++) {
        int16_t vy = l->y + dy;
        if (vy < 0 || vy >= HEIGHT)
            continue;

        // Pre-calculate values for this scanline
        uint16_t map_width = WIDTH >> 3; // WIDTH / 8
        uint16_t vy_div8 = vy >> 3;
        uint8_t layer_z = l->z;

        int16_t startdx = -1;
        uint16_t consecutive_count = 0;

        // Process in 8-pixel blocks for efficiency, but merge consecutive
        // blocks
        for (int16_t dx = aligned_dx0; dx < aligned_dx1; dx += 8) {
            uint16_t vx = l->x + dx;
            if (vx >= WIDTH)
                break;

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
                // End of visible region - perform CPU transfer for merged
                // blocks
                uint16_t transfer_x = l->x + startdx;
                uint16_t actual_start = (startdx < dx0) ? dx0 : startdx;
                uint16_t actual_end = ((startdx + consecutive_count) > dx1)
                                          ? dx1
                                          : (startdx + consecutive_count);
                uint16_t transfer_width = actual_end - actual_start;

                if (transfer_width > 0) {
                    uint8_t* src = &l->vram[dy * l->w + actual_start];
                    volatile uint16_t* vram =
                        &vram_start[vy * VRAMWIDTH + l->x + actual_start];
                    ss_layer_blit_to_vram(vram, 0, src, transfer_width);
                }

                startdx = -1;
                consecutive_count = 0;
            }
        }

        // Handle final merged region if it extends to the end
        if (startdx >= 0) {
            uint16_t transfer_x = l->x + startdx;
            uint16_t actual_start = (startdx < dx0) ? dx0 : startdx;
            uint16_t actual_end = ((startdx + consecutive_count) > dx1)
                                      ? dx1
                                      : (startdx + consecutive_count);
            uint16_t transfer_width = actual_end - actual_start;

            if (transfer_width > 0) {
                uint8_t* src = &l->vram[dy * l->w + actual_start];
                volatile uint16_t* vram =
                    &vram_start[vy * VRAMWIDTH + l->x + actual_start];
                ss_layer_blit_to_vram(vram, 0, src, transfer_width);
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

                // Draw the dirty rectangle using the new optimized
                // scanline-based method
                ss_layer_draw_rect_layer_bounds(
                    layer, layer->dirty_x, layer->dirty_y,
                    layer->dirty_x + layer->dirty_w,
                    layer->dirty_y + layer->dirty_h);

                SS_PERF_END_MEASUREMENT(SS_PERF_DIRTY_RECT);
            } else {
                // Performance monitoring: Start full layer drawing
                SS_PERF_START_MEASUREMENT(SS_PERF_FULL_LAYER);

                // For initial draw or full layer invalidation, use optimized
                // method too
                ss_layer_draw_rect_layer_bounds(layer, 0, 0, layer->w,
                                                layer->h);

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
        if ((layer->attr & LAYER_ATTR_VISIBLE) && x >= layer->x &&
            x < layer->x + layer->w && y >= layer->y &&
            y < layer->y + layer->h) {
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
    if (!layer || !(layer->attr & LAYER_ATTR_USED) ||
        new_z >= ss_layer_mgr->topLayerIdx) {
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
