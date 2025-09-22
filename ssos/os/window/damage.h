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

// Alignment helpers for 8-pixel boundaries (X68000 optimal)
static inline uint16_t ss_damage_align8(uint16_t value) {
    return value & 0xFFF8;
}

static inline uint16_t ss_damage_align8_ceil(uint16_t value) {
    return (value + 7) & 0xFFF8;
}