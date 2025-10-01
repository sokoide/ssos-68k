#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "ss_config.h"

// Maximum number of damage regions to track (X11 DamageExt compatible)
#define MAX_DAMAGE_REGIONS 32

// Damage rectangle structure - X11 DamageExt compatible
typedef struct {
    uint16_t x, y;           // Region origin (8-pixel aligned)
    uint16_t w, h;           // Region dimensions (8-pixel aligned)
    bool needs_redraw;       // Whether this region needs redrawing
} DamageRect;

// Enhanced damage rectangle with occlusion tracking
typedef struct {
    DamageRect base;                    // Base damage rectangle
    uint32_t occlusion_percentage;      // Percentage of area occluded (0-100)
    bool is_partially_occluded;         // True if 0% < occlusion < 100%
    uint8_t occluding_layer_count;      // Number of layers occluding this region
    // Store indices of occluding layers for quick reference
    uint8_t occluding_layer_indices[MAX_LAYERS];
} EnhancedDamageRect;

// Damage buffer structure for efficient dirty region management
typedef struct {
    uint8_t* buffer;                    // Offscreen buffer (384KB for 768x512)
    DamageRect regions[MAX_DAMAGE_REGIONS]; // Damage regions array
    int region_count;                   // Current number of active regions
    uint16_t buffer_width, buffer_height; // Buffer dimensions
    bool buffer_allocated;              // Whether buffer is allocated
} DamageBuffer;

// Global damage buffer instance
extern DamageBuffer g_damage_buffer;

// Damage buffer management functions
void ss_damage_init();
void ss_damage_cleanup();
void ss_damage_reset();

// Damage region operations
void ss_damage_add_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ss_damage_merge_regions();
void ss_damage_clear_regions();

// Drawing operations using damage buffer
void ss_damage_draw_regions();
void ss_damage_optimize_for_occlusion();

// Occlusion optimization functions
uint32_t ss_damage_calculate_occlusion_fraction(const DamageRect* region,
                                                uint8_t* occluding_layers,
                                                uint8_t* layer_count);
bool ss_damage_should_split_region(uint32_t occlusion_percentage);
bool ss_damage_is_region_fully_occluded(const DamageRect* region);

// Occlusion configuration for adaptive thresholds
typedef struct {
    uint32_t full_occlusion_threshold;      // Default: 95%
    uint32_t split_threshold;               // Default: 50%
    uint32_t max_enhanced_regions;          // Default: 8
    uint32_t performance_samples;
    uint32_t avg_processing_time;
} OcclusionConfig;

extern OcclusionConfig g_occlusion_config;

// Utility functions
bool ss_damage_rects_overlap(const DamageRect* a, const DamageRect* b);
void ss_damage_merge_rects(DamageRect* dest, const DamageRect* src);
bool ss_damage_is_rect_empty(const DamageRect* rect);

// Performance monitoring
typedef struct {
    uint32_t total_regions_processed;
    uint32_t total_pixels_drawn;
    uint32_t dma_transfers_count;
    uint32_t cpu_transfers_count;
    uint32_t last_report_time;
} DamagePerfStats;

extern DamagePerfStats g_damage_perf;

void ss_damage_perf_reset();
void ss_damage_perf_report();
void ss_damage_perf_update(uint32_t pixels_drawn, bool used_dma);

// Occlusion performance reporting
void ss_damage_occlusion_report();

// Alignment helpers for 8-pixel boundaries (X68000 optimal)
static inline uint16_t ss_damage_align8(uint16_t value) {
    return value & 0xFFF8;
}

static inline uint16_t ss_damage_align8_ceil(uint16_t value) {
    return (value + 7) & 0xFFF8;
}