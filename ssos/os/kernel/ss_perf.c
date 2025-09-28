#include "ss_perf.h"

#include "kernel.h"

// Global performance monitor instance
SsPerformanceMonitor ss_perf_monitor = {0};

// Timing measurement data
typedef struct {
    uint32_t start_time;
    uint32_t total_time;
    uint32_t measurement_count;
    uint32_t max_time;
    uint32_t min_time;
} SsTimingMetric;

static SsTimingMetric ss_timing_metrics[SS_PERF_MAX_METRICS];

// Initialize performance monitoring
void ss_perf_init(void) {
    // Clear all performance data
    for (int i = 0; i < SS_CONFIG_PERF_MAX_SAMPLES; i++) {
        ss_perf_monitor.samples[i].timestamp = 0;
        ss_perf_monitor.samples[i].interrupt_count = 0;
        ss_perf_monitor.samples[i].context_switches = 0;
        ss_perf_monitor.samples[i].memory_allocations = 0;
        ss_perf_monitor.samples[i].dma_transfers = 0;
        ss_perf_monitor.samples[i].font_render_ops = 0;
        ss_perf_monitor.samples[i].cpu_idle_time = 0;
    }

    ss_perf_monitor.current_sample = 0;
    ss_perf_monitor.sample_count = 0;
    ss_perf_monitor.last_sample_time = 0;
    ss_perf_monitor.total_interrupts = 0;
    ss_perf_monitor.total_context_switches = 0;
    ss_perf_monitor.total_memory_ops = 0;
    ss_perf_monitor.total_graphics_ops = 0;
    ss_perf_monitor.system_start_time = ss_timerd_counter;

    // Initialize timing metrics
    for (int i = 0; i < SS_CONFIG_PERF_MAX_METRICS; i++) {
        ss_timing_metrics[i].start_time = 0;
        ss_timing_metrics[i].total_time = 0;
        ss_timing_metrics[i].measurement_count = 0;
        ss_timing_metrics[i].max_time = 0;
        ss_timing_metrics[i].min_time = 0;
    }
}

// Take a performance sample
void ss_perf_sample(void) {
    uint32_t current_time = ss_timerd_counter;

    // Only sample at configured intervals
    if (current_time - ss_perf_monitor.last_sample_time <
        SS_CONFIG_PERF_SAMPLE_INTERVAL) {
        return;
    }

    // Store current sample
    SsPerformanceSample* sample =
        &ss_perf_monitor.samples[ss_perf_monitor.current_sample];

    sample->timestamp = current_time;
    sample->interrupt_count = ss_perf_monitor.total_interrupts;
    sample->context_switches = ss_perf_monitor.total_context_switches;
    sample->memory_allocations = ss_perf_monitor.total_memory_ops;
    sample->dma_transfers = 0;  // Would be tracked separately
    sample->font_render_ops = ss_perf_monitor.total_graphics_ops;
    sample->cpu_idle_time = 0;  // Would need separate tracking

    // Update counters
    ss_perf_monitor.current_sample =
        (ss_perf_monitor.current_sample + 1) % SS_CONFIG_PERF_MAX_SAMPLES;
    if (ss_perf_monitor.sample_count < SS_CONFIG_PERF_MAX_SAMPLES) {
        ss_perf_monitor.sample_count++;
    }
    ss_perf_monitor.last_sample_time = current_time;
}

// Performance counter increment functions
void ss_perf_increment_interrupt(void) {
    ss_perf_monitor.total_interrupts++;
}

void ss_perf_increment_context_switch(void) {
    ss_perf_monitor.total_context_switches++;
}

void ss_perf_increment_memory_op(void) {
    ss_perf_monitor.total_memory_ops++;
}

void ss_perf_increment_graphics_op(void) {
    ss_perf_monitor.total_graphics_ops++;
}

// Get current performance sample
SsPerformanceSample ss_perf_get_current(void) {
    SsPerformanceSample current = {0};
    current.timestamp = ss_timerd_counter;
    current.interrupt_count = ss_perf_monitor.total_interrupts;
    current.context_switches = ss_perf_monitor.total_context_switches;
    current.memory_allocations = ss_perf_monitor.total_memory_ops;
    current.dma_transfers = 0;
    current.font_render_ops = ss_perf_monitor.total_graphics_ops;
    current.cpu_idle_time = 0;

    return current;
}

// Get average performance over all samples
SsPerformanceSample ss_perf_get_average(void) {
    SsPerformanceSample average = {0};

    if (ss_perf_monitor.sample_count == 0) {
        return average;
    }

    // Calculate averages
    for (uint32_t i = 0; i < ss_perf_monitor.sample_count; i++) {
        average.interrupt_count += ss_perf_monitor.samples[i].interrupt_count;
        average.context_switches += ss_perf_monitor.samples[i].context_switches;
        average.memory_allocations +=
            ss_perf_monitor.samples[i].memory_allocations;
        average.dma_transfers += ss_perf_monitor.samples[i].dma_transfers;
        average.font_render_ops += ss_perf_monitor.samples[i].font_render_ops;
        average.cpu_idle_time += ss_perf_monitor.samples[i].cpu_idle_time;
    }

    // Divide by sample count
    uint32_t count = ss_perf_monitor.sample_count;
    average.interrupt_count /= count;
    average.context_switches /= count;
    average.memory_allocations /= count;
    average.dma_transfers /= count;
    average.font_render_ops /= count;
    average.cpu_idle_time /= count;
    average.timestamp = ss_timerd_counter;

    return average;
}

// Get system uptime in timer ticks
uint32_t ss_perf_get_uptime(void) {
    return ss_timerd_counter - ss_perf_monitor.system_start_time;
}

// Timing measurement functions
void ss_perf_start_measurement(uint32_t metric_id) {
    if (metric_id >= SS_PERF_MAX_METRICS) {
        return;
    }

    ss_timing_metrics[metric_id].start_time = ss_timerd_counter;
}

void ss_perf_end_measurement(uint32_t metric_id) {
    if (metric_id >= SS_PERF_MAX_METRICS) {
        return;
    }

    uint32_t end_time = ss_timerd_counter;
    SsTimingMetric* metric = &ss_timing_metrics[metric_id];
    uint32_t duration = end_time - metric->start_time;

    // Update timing statistics
    metric->total_time += duration;
    metric->measurement_count++;

    if (metric->measurement_count == 1) {
        metric->min_time = duration;
        metric->max_time = duration;
    } else {
        if (duration < metric->min_time) {
            metric->min_time = duration;
        }
        if (duration > metric->max_time) {
            metric->max_time = duration;
        }
    }
}

uint32_t ss_perf_get_measurement(uint32_t metric_id) {
    if (metric_id >= SS_PERF_MAX_METRICS ||
        ss_timing_metrics[metric_id].measurement_count == 0) {
        return 0;
    }

    return ss_timing_metrics[metric_id].total_time /
           ss_timing_metrics[metric_id].measurement_count;
}

uint32_t ss_perf_get_average_measurement(uint32_t metric_id) {
    return ss_perf_get_measurement(
        metric_id);  // Same as get_measurement for now
}