#pragma once

#include <stdint.h>

// Performance monitoring system for SSOS
// Provides basic performance tracking and profiling capabilities

typedef struct {
    uint32_t timestamp;
    uint32_t interrupt_count;
    uint32_t context_switches;
    uint32_t memory_allocations;
    uint32_t dma_transfers;
    uint32_t font_render_ops;
    uint32_t cpu_idle_time;
} SsPerformanceSample;

// Performance monitoring configuration - using centralized config
#include "ss_config.h"

typedef struct {
    SsPerformanceSample samples[SS_CONFIG_PERF_MAX_SAMPLES];
    uint32_t current_sample;
    uint32_t sample_count;
    uint32_t last_sample_time;

    // Performance counters
    uint32_t total_interrupts;
    uint32_t total_context_switches;
    uint32_t total_memory_ops;
    uint32_t total_graphics_ops;
    uint32_t system_start_time;
} SsPerformanceMonitor;

// Global performance monitor instance
extern SsPerformanceMonitor ss_perf_monitor;

// Performance monitoring functions
void ss_perf_init(void);
void ss_perf_sample(void);
void ss_perf_increment_interrupt(void);
void ss_perf_increment_context_switch(void);
void ss_perf_increment_memory_op(void);
void ss_perf_increment_graphics_op(void);
SsPerformanceSample ss_perf_get_current(void);
SsPerformanceSample ss_perf_get_average(void);
uint32_t ss_perf_get_uptime(void);

// Performance metric IDs for timing measurements
#define SS_PERF_FRAME_TIME      0
#define SS_PERF_LAYER_UPDATE    1
#define SS_PERF_DRAW_TIME       2
#define SS_PERF_DIRTY_DRAW      3
#define SS_PERF_DIRTY_RECT      4
#define SS_PERF_FULL_LAYER      5
#define SS_PERF_MEMORY_OP       6
#define SS_PERF_QD_FRAME_TIME   7
#define SS_PERF_QD_UPDATE       8
#define SS_PERF_QD_DRAW_TIME    9
// SS_PERF_MAX_METRICS defined in ss_config.h as an alias

// Timing measurement functions
void ss_perf_start_measurement(uint32_t metric_id);
void ss_perf_end_measurement(uint32_t metric_id);
uint32_t ss_perf_get_measurement(uint32_t metric_id);
uint32_t ss_perf_get_average_measurement(uint32_t metric_id);

// Performance macros for easy instrumentation
#if SS_CONFIG_ENABLE_PERFORMANCE_MONITORING
#define SS_PERF_INC_INTERRUPTS()     ss_perf_increment_interrupt()
#define SS_PERF_INC_CONTEXT_SWITCHES() ss_perf_increment_context_switch()
#define SS_PERF_INC_MEMORY_OPS()     ss_perf_increment_memory_op()
#define SS_PERF_INC_GRAPHICS_OPS()   ss_perf_increment_graphics_op()
#define SS_PERF_START_MEASUREMENT(id) ss_perf_start_measurement(id)
#define SS_PERF_END_MEASUREMENT(id)  ss_perf_end_measurement(id)
#else
#define SS_PERF_INC_INTERRUPTS()     (void)0
#define SS_PERF_INC_CONTEXT_SWITCHES() (void)0
#define SS_PERF_INC_MEMORY_OPS()     (void)0
#define SS_PERF_INC_GRAPHICS_OPS()   (void)0
#define SS_PERF_START_MEASUREMENT(id) (void)0
#define SS_PERF_END_MEASUREMENT(id)  (void)0
#endif
