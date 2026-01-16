#include "layer.h"
#include "damage.h"
#include "memory.h"
#include "kernel.h"
#include "ss_perf.h"
#include <stddef.h>

// Internal helper function declarations
static int ss_damage_overlap_area(const DamageRect* rect, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
static bool ss_damage_layer_overlaps_region(const Layer* layer, const DamageRect* region);
static void ss_damage_calculate_layer_region_overlap(const Layer* layer, const DamageRect* region,
                                                    uint16_t* overlap_x, uint16_t* overlap_y,
                                                    uint16_t* overlap_w, uint16_t* overlap_h);
static void ss_damage_draw_layer_region(const Layer* layer, uint16_t x, uint16_t y, uint16_t w, uint16_t h);

// Global damage buffer instance
DamageBuffer g_damage_buffer;
DamagePerfStats g_damage_perf;

// Global occlusion configuration - CONSERVATIVE VALUES FOR DEBUGGING
OcclusionConfig g_occlusion_config = {
    .full_occlusion_threshold = 100,   // 100% occlusion needed to mark as fully occluded (DISABLED)
    .split_threshold = 75,             // 75% occlusion needed to consider splitting
    .max_enhanced_regions = 8,         // Maximum number of enhanced regions to process
    .performance_samples = 0,
    .avg_processing_time = 0
};

// Initialize the damage buffer system
void ss_damage_init() {
    // Initialize buffer dimensions
    g_damage_buffer.buffer_width = 768;   // X68000 standard width
    g_damage_buffer.buffer_height = 512;  // X68000 standard height

    // Allocate 384KB offscreen buffer (768x512 bytes for 8-bit indexed color)
    g_damage_buffer.buffer = (uint8_t*)ss_mem_alloc4k(g_damage_buffer.buffer_width * g_damage_buffer.buffer_height);
    if (g_damage_buffer.buffer) {
        g_damage_buffer.buffer_allocated = true;
        // Clear buffer to black
        for (int i = 0; i < g_damage_buffer.buffer_width * g_damage_buffer.buffer_height; i++) {
            g_damage_buffer.buffer[i] = 0;
        }
    } else {
        g_damage_buffer.buffer_allocated = false;
    }

    // Initialize damage regions
    g_damage_buffer.region_count = 0;
    for (int i = 0; i < MAX_DAMAGE_REGIONS; i++) {
        g_damage_buffer.regions[i].needs_redraw = false;
        g_damage_buffer.regions[i].w = 0;
        g_damage_buffer.regions[i].h = 0;
    }

    // Initialize performance monitoring
    ss_damage_perf_reset();
}

// Cleanup damage buffer system
void ss_damage_cleanup() {
    if (g_damage_buffer.buffer_allocated && g_damage_buffer.buffer) {
        // Note: We don't free the memory as it's managed by the memory allocator
        g_damage_buffer.buffer = NULL;
        g_damage_buffer.buffer_allocated = false;
    }
    g_damage_buffer.region_count = 0;
}

// Reset all damage regions for a new frame
void ss_damage_reset() {
    g_damage_buffer.region_count = 0;
    for (int i = 0; i < MAX_DAMAGE_REGIONS; i++) {
        g_damage_buffer.regions[i].needs_redraw = false;
        g_damage_buffer.regions[i].w = 0;
        g_damage_buffer.regions[i].h = 0;
    }
}

// Add a rectangular region to the damage buffer
void ss_damage_add_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (w == 0 || h == 0) return;

    // Align to 8-pixel boundaries for optimal X68000 performance
    uint16_t aligned_x = ss_damage_align8(x);
    uint16_t aligned_y = ss_damage_align8(y);
    uint16_t aligned_w = ss_damage_align8_ceil(x + w) - aligned_x;
    uint16_t aligned_h = ss_damage_align8_ceil(y + h) - aligned_y;

    // Clamp to buffer bounds
    if (aligned_x >= g_damage_buffer.buffer_width) return;
    if (aligned_y >= g_damage_buffer.buffer_height) return;

    if (aligned_x + aligned_w > g_damage_buffer.buffer_width) {
        aligned_w = g_damage_buffer.buffer_width - aligned_x;
    }
    if (aligned_y + aligned_h > g_damage_buffer.buffer_height) {
        aligned_h = g_damage_buffer.buffer_height - aligned_y;
    }

    if (aligned_w == 0 || aligned_h == 0) return;

    // Try to merge with existing regions first
    for (int i = 0; i < g_damage_buffer.region_count; i++) {
        if (g_damage_buffer.regions[i].needs_redraw) {
            DamageRect* existing = &g_damage_buffer.regions[i];

            // Check if rectangles overlap significantly (more than 75% overlap)
            // Increased threshold from 50% to 75% for finer-grained tracking
            int overlap_area = ss_damage_overlap_area(existing, aligned_x, aligned_y, aligned_w, aligned_h);
            int existing_area = existing->w * existing->h;
            int new_area = aligned_w * aligned_h;

            // Only merge if overlap is more than 75% of either region
            if (overlap_area > (existing_area * 3 / 4) || overlap_area > (new_area * 3 / 4)) {
                // Merge with existing region
                uint16_t x1 = existing->x;
                uint16_t y1 = existing->y;
                uint16_t x2 = existing->x + existing->w;
                uint16_t y2 = existing->y + existing->h;

                uint16_t new_x1 = (aligned_x < x1) ? aligned_x : x1;
                uint16_t new_y1 = (aligned_y < y1) ? aligned_y : y1;
                uint16_t new_x2 = ((aligned_x + aligned_w) > x2) ? (aligned_x + aligned_w) : x2;
                uint16_t new_y2 = ((aligned_y + aligned_h) > y2) ? (aligned_y + aligned_h) : y2;

                existing->x = new_x1;
                existing->y = new_y1;
                existing->w = new_x2 - new_x1;
                existing->h = new_y2 - new_y1;
                return;
            }
        }
    }

    // Add as new region if space available
    if (g_damage_buffer.region_count < MAX_DAMAGE_REGIONS) {
        DamageRect* new_rect = &g_damage_buffer.regions[g_damage_buffer.region_count];
        new_rect->x = aligned_x;
        new_rect->y = aligned_y;
        new_rect->w = aligned_w;
        new_rect->h = aligned_h;
        new_rect->needs_redraw = true;
        g_damage_buffer.region_count++;
    } else {
        // No space for new regions, merge with the largest existing region
        int max_area_idx = 0;
        int max_area = 0;
        for (int i = 0; i < MAX_DAMAGE_REGIONS; i++) {
            int area = g_damage_buffer.regions[i].w * g_damage_buffer.regions[i].h;
            if (area > max_area) {
                max_area = area;
                max_area_idx = i;
            }
        }

        // Merge with the largest region
        DamageRect* largest = &g_damage_buffer.regions[max_area_idx];
        uint16_t x1 = largest->x;
        uint16_t y1 = largest->y;
        uint16_t x2 = largest->x + largest->w;
        uint16_t y2 = largest->y + largest->h;

        uint16_t new_x1 = (aligned_x < x1) ? aligned_x : x1;
        uint16_t new_y1 = (aligned_y < y1) ? aligned_y : y1;
        uint16_t new_x2 = ((aligned_x + aligned_w) > x2) ? (aligned_x + aligned_w) : x2;
        uint16_t new_y2 = ((aligned_y + aligned_h) > y2) ? (aligned_y + aligned_h) : y2;

        largest->x = new_x1;
        largest->y = new_y1;
        largest->w = new_x2 - new_x1;
        largest->h = new_y2 - new_y1;
    }
}

// Merge overlapping damage regions to optimize drawing
void ss_damage_merge_regions() {
    // Simple merging algorithm - combine overlapping regions
    bool changed = true;
    while (changed && g_damage_buffer.region_count > 1) {
        changed = false;

        for (int i = 0; i < g_damage_buffer.region_count - 1; i++) {
            for (int j = i + 1; j < g_damage_buffer.region_count; j++) {
                DamageRect* rect1 = &g_damage_buffer.regions[i];
                DamageRect* rect2 = &g_damage_buffer.regions[j];

                if (rect1->needs_redraw && rect2->needs_redraw) {
                    if (ss_damage_rects_overlap(rect1, rect2)) {
                        // Merge rect2 into rect1
                        ss_damage_merge_rects(rect1, rect2);

                        // Shift remaining regions down
                        for (int k = j; k < g_damage_buffer.region_count - 1; k++) {
                            g_damage_buffer.regions[k] = g_damage_buffer.regions[k + 1];
                        }
                        g_damage_buffer.region_count--;
                        changed = true;
                        break;
                    }
                }
            }
            if (changed) break;
        }
    }
}

// Clear all damage regions
void ss_damage_clear_regions() {
    g_damage_buffer.region_count = 0;
    for (int i = 0; i < MAX_DAMAGE_REGIONS; i++) {
        g_damage_buffer.regions[i].needs_redraw = false;
    }
}

// Draw all damage regions - main drawing loop
void ss_damage_draw_regions() {
    SS_PERF_START_MEASUREMENT(SS_PERF_DAMAGE_DRAW);

    // Optimize regions before drawing
    ss_damage_merge_regions();
    ss_damage_optimize_for_occlusion();

    // Draw each damage region
    for (int i = 0; i < g_damage_buffer.region_count; i++) {
        DamageRect* region = &g_damage_buffer.regions[i];
        if (region->needs_redraw && region->w > 0 && region->h > 0) {

            // Draw layers in z-order for this region
            for (int layer_idx = 0; layer_idx < ss_layer_mgr->topLayerIdx; layer_idx++) {
                Layer* layer = ss_layer_mgr->zLayers[layer_idx];

                // Check if layer overlaps with damage region
                if (ss_damage_layer_overlaps_region(layer, region)) {
                    // Calculate overlap rectangle
                    uint16_t overlap_x, overlap_y, overlap_w, overlap_h;
                    ss_damage_calculate_layer_region_overlap(layer, region,
                                                            &overlap_x, &overlap_y,
                                                            &overlap_w, &overlap_h);

                    if (overlap_w > 0 && overlap_h > 0) {
                        if (!ss_layer_region_visible(layer, overlap_x, overlap_y, overlap_w, overlap_h)) {
                            g_damage_perf.occlusion_culled_regions++;
                            continue;
                        }

                        // Draw the overlapped portion of this layer
                        ss_damage_draw_layer_region(layer, overlap_x, overlap_y, overlap_w, overlap_h);

                        // Update performance statistics
                        uint32_t pixels_drawn = (uint32_t)overlap_w * (uint32_t)overlap_h;
                        g_damage_perf.total_pixels_drawn += pixels_drawn;
                        g_damage_perf.total_regions_processed++;
                    }
                }
            }

            region->needs_redraw = false;
        }
    }

    // Clear regions for next frame
    ss_damage_clear_regions();

    SS_PERF_END_MEASUREMENT(SS_PERF_DAMAGE_DRAW);

    // Report performance every 1000 operations
    if (g_damage_perf.total_regions_processed > 0 &&
        (g_damage_perf.total_regions_processed % 1000) == 0) {
        ss_damage_perf_report();
    }
}

// Optimize damage regions by removing occluded areas
// TEMPORARILY DISABLED: Hybrid scanline-based occlusion optimization
void ss_damage_optimize_for_occlusion() {
    // DISABLED FOR DEBUGGING: The occlusion optimization might be blocking layer 1 updates
    // This ensures all damage regions are processed to debug the issue
    return;
}

// Check if two damage rectangles overlap
bool ss_damage_rects_overlap(const DamageRect* a, const DamageRect* b) {
    return !(a->x >= b->x + b->w ||
             b->x >= a->x + a->w ||
             a->y >= b->y + b->h ||
             b->y >= a->y + a->h);
}

// Merge rectangle b into rectangle a
void ss_damage_merge_rects(DamageRect* dest, const DamageRect* src) {
    uint16_t x1 = dest->x;
    uint16_t y1 = dest->y;
    uint16_t x2 = dest->x + dest->w;
    uint16_t y2 = dest->y + dest->h;

    uint16_t new_x1 = (src->x < x1) ? src->x : x1;
    uint16_t new_y1 = (src->y < y1) ? src->y : y1;
    uint16_t new_x2 = ((src->x + src->w) > x2) ? (src->x + src->w) : x2;
    uint16_t new_y2 = ((src->y + src->h) > y2) ? (src->y + src->h) : y2;

    dest->x = new_x1;
    dest->y = new_y1;
    dest->w = new_x2 - new_x1;
    dest->h = new_y2 - new_y1;
}

// Check if a rectangle is empty
bool ss_damage_is_rect_empty(const DamageRect* rect) {
    return rect->w == 0 || rect->h == 0 || !rect->needs_redraw;
}

// Reset performance statistics
void ss_damage_perf_reset() {
    g_damage_perf.total_regions_processed = 0;
    g_damage_perf.total_pixels_drawn = 0;
    g_damage_perf.dma_transfers_count = 0;
    g_damage_perf.cpu_transfers_count = 0;
    g_damage_perf.last_report_time = ss_timerd_counter;
    g_damage_perf.occlusion_culled_regions = 0;
}

// Report performance statistics
void ss_damage_perf_report() {
    uint32_t current_time = ss_timerd_counter;
    uint32_t elapsed = current_time - g_damage_perf.last_report_time;

    if (elapsed > 0 && g_damage_perf.total_regions_processed > 0) {
        // Calculate average pixels per operation
        uint32_t avg_pixels = g_damage_perf.total_pixels_drawn / g_damage_perf.total_regions_processed;

        // Calculate transfer method percentages
        uint32_t total_transfers = g_damage_perf.dma_transfers_count + g_damage_perf.cpu_transfers_count;
        uint32_t dma_percent = total_transfers > 0 ?
            (g_damage_perf.dma_transfers_count * 100) / total_transfers : 0;
        uint32_t culled_regions = g_damage_perf.occlusion_culled_regions;

        // Simple performance output (can be enhanced with proper printf)
        // Performance: %d regions, %d avg pixels, DMA: %d%%, culled: %d
        // This would need proper string output implementation
    }
}

// Update performance statistics
void ss_damage_perf_update(uint32_t pixels_drawn, bool used_dma) {
    g_damage_perf.total_pixels_drawn += pixels_drawn;
    if (used_dma) {
        g_damage_perf.dma_transfers_count++;
    } else {
        g_damage_perf.cpu_transfers_count++;
    }
}

// Report occlusion optimization statistics
void ss_damage_occlusion_report() {
    // This would display occlusion statistics in a real implementation
    // For now, we can track the metrics internally
    // The performance impact can be measured by comparing regions processed vs regions drawn

    // Calculate occlusion efficiency: if avg processing time is low and regions are removed,
    // then the optimization is effective
    if (g_occlusion_config.performance_samples > 0) {
        // Simple efficiency metric (could be enhanced)
        uint32_t efficiency = g_damage_perf.total_regions_processed /
                             (g_occlusion_config.avg_processing_time + 1);

        // This data could be used to adapt thresholds in future versions
        // For now, it's collected for debugging purposes
    }
}

// Helper function to calculate overlap area
static int ss_damage_overlap_area(const DamageRect* rect, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    int x1 = (rect->x > x) ? rect->x : x;
    int y1 = (rect->y > y) ? rect->y : y;
    int x2 = ((rect->x + rect->w) < (x + w)) ? (rect->x + rect->w) : (x + w);
    int y2 = ((rect->y + rect->h) < (y + h)) ? (rect->y + rect->h) : (y + h);

    if (x2 > x1 && y2 > y1) {
        return (x2 - x1) * (y2 - y1);
    }
    return 0;
}

// Check if layer overlaps with damage region
static bool ss_damage_layer_overlaps_region(const Layer* layer, const DamageRect* region) {
    return !(layer->x >= region->x + region->w ||
             region->x >= layer->x + layer->w ||
             layer->y >= region->y + region->h ||
             region->y >= layer->y + layer->h);
}

// Calculate overlap between layer and damage region
static void ss_damage_calculate_layer_region_overlap(const Layer* layer, const DamageRect* region,
                                                    uint16_t* overlap_x, uint16_t* overlap_y,
                                                    uint16_t* overlap_w, uint16_t* overlap_h) {
    uint16_t x1 = (layer->x > region->x) ? layer->x : region->x;
    uint16_t y1 = (layer->y > region->y) ? layer->y : region->y;
    uint16_t x2 = ((layer->x + layer->w) < (region->x + region->w)) ?
                   (layer->x + layer->w) : (region->x + region->w);
    uint16_t y2 = ((layer->y + layer->h) < (region->y + region->h)) ?
                   (layer->y + layer->h) : (region->y + region->h);

    *overlap_x = x1 - layer->x;  // Convert to layer-local coordinates
    *overlap_y = y1 - layer->y;
    *overlap_w = x2 - x1;
    *overlap_h = y2 - y1;
}

// Calculate the percentage of a damage region that is occluded by upper layers
uint32_t ss_damage_calculate_occlusion_fraction(const DamageRect* region,
                                                uint8_t* occluding_layers,
                                                uint8_t* layer_count) {
    uint32_t total_occluded_area = 0;
    uint32_t region_area = region->w * region->h;

    if (region_area == 0) return 0;

    *layer_count = 0;

    // SAFER IMPLEMENTATION: Only optimize very obvious cases
    // For now, we'll only mark regions as occluded if they are clearly covered
    // by layers that are static (don't change every frame)

    // Check if layer 0 (background/taskbar) completely covers this region
    // and the region is outside the window areas
    if (ss_layer_mgr->topLayerIdx > 0) {
        Layer* background_layer = ss_layer_mgr->zLayers[0];

        // If this region is completely within the background layer area
        // and doesn't overlap with any window, we can potentially optimize
        if (background_layer->x <= region->x &&
            background_layer->y <= region->y &&
            background_layer->x + background_layer->w >= region->x + region->w &&
            background_layer->y + background_layer->h >= region->y + region->h) {

            // Check if this region overlaps with any other visible layer
            bool overlaps_with_window = false;
            for (int layer_idx = 1; layer_idx < ss_layer_mgr->topLayerIdx; layer_idx++) {
                Layer* window_layer = ss_layer_mgr->zLayers[layer_idx];
                if (!(window_layer->attr & LAYER_ATTR_VISIBLE)) continue;

                // Simple overlap check
                if (!(window_layer->x >= region->x + region->w ||
                     window_layer->x + window_layer->w <= region->x ||
                     window_layer->y >= region->y + region->h ||
                     window_layer->y + window_layer->h <= region->y)) {
                    overlaps_with_window = true;
                    break;
                }
            }

            // If the region is in background area and doesn't overlap with any window
            // and the background is static, we can potentially optimize
            if (!overlaps_with_window) {
                total_occluded_area = region_area;
            }
        }
    }

    // For now, return 0 (no occlusion) for most cases to be safe
    // This prevents the optimization from blocking updates
    return (total_occluded_area * 100) / region_area;
}

// Check if a region should be split based on occlusion percentage
bool ss_damage_should_split_region(uint32_t occlusion_percentage) {
    return occlusion_percentage >= g_occlusion_config.split_threshold &&
           occlusion_percentage < g_occlusion_config.full_occlusion_threshold;
}

// Check if a region is fully occluded (above threshold)
bool ss_damage_is_region_fully_occluded(const DamageRect* region) {
    uint8_t occluding_layers[MAX_LAYERS];
    uint8_t layer_count;

    uint32_t occlusion_percentage = ss_damage_calculate_occlusion_fraction(
        region, occluding_layers, &layer_count);

    return occlusion_percentage >= g_occlusion_config.full_occlusion_threshold;
}

// Draw a specific region of a layer
static void ss_damage_draw_layer_region(const Layer* layer, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // Use the existing layer drawing system with the optimized DMA functions
    ss_layer_draw_rect_layer_bounds((Layer*)layer, x, y, x + w, y + h);
}
