#include "ss_config.h"
#include "kernel.h"

// Global runtime configuration instance
SsRuntimeConfig ss_runtime_config = {0};

// Initialize runtime configuration with default values
SsConfigResult ss_config_init(void) {
    // Initialize with compile-time defaults
    ss_runtime_config.max_tasks = SS_CONFIG_MAX_TASKS;
    ss_runtime_config.max_task_pri = SS_CONFIG_MAX_TASK_PRI;
    ss_runtime_config.task_stack_size = SS_CONFIG_TASK_STACK_SIZE;

    ss_runtime_config.memory_block_size = SS_CONFIG_MEMORY_BLOCK_SIZE;
    ss_runtime_config.memory_blocks = SS_CONFIG_MEMORY_BLOCKS;
    ss_runtime_config.memory_total_size = SS_CONFIG_MEMORY_TOTAL_SIZE;

    ss_runtime_config.perf_sample_interval = SS_CONFIG_PERF_SAMPLE_INTERVAL;
    ss_runtime_config.perf_max_samples = SS_CONFIG_PERF_MAX_SAMPLES;
    ss_runtime_config.perf_max_metrics = SS_CONFIG_PERF_MAX_METRICS;

    ss_runtime_config.max_layers = SS_CONFIG_MAX_LAYERS;
    ss_runtime_config.key_buffer_size = SS_CONFIG_KEY_BUFFER_SIZE;
    ss_runtime_config.context_switch_interval = SS_CONFIG_CONTEXT_SWITCH_INTERVAL;

    return SS_CONFIG_OK;
}

// Set task system limits with validation
SsConfigResult ss_config_set_task_limits(uint16_t max_tasks, uint16_t max_priority) {
    if (max_tasks == 0 || max_tasks > 256) {
        return SS_CONFIG_INVALID_TASKS;
    }

    if (max_priority == 0 || max_priority > 32) {
        return SS_CONFIG_INVALID_PRIORITY;
    }

    ss_runtime_config.max_tasks = max_tasks;
    ss_runtime_config.max_task_pri = max_priority;

    return SS_CONFIG_OK;
}

// Set memory system limits with validation
SsConfigResult ss_config_set_memory_limits(uint32_t block_size, uint32_t blocks) {
    if (block_size < 256 || block_size > (1024 * 1024)) {
        return SS_CONFIG_INVALID_MEMORY;
    }

    if (blocks == 0 || blocks > 8192) {
        return SS_CONFIG_INVALID_MEMORY;
    }

    ss_runtime_config.memory_block_size = block_size;
    ss_runtime_config.memory_blocks = blocks;
    ss_runtime_config.memory_total_size = block_size * blocks;

    return SS_CONFIG_OK;
}

// Set performance monitoring limits with validation
SsConfigResult ss_config_set_performance_limits(uint32_t sample_interval, uint32_t max_samples) {
    if (sample_interval < 10 || sample_interval > 100000) {
        return SS_CONFIG_INVALID_PERFORMANCE;
    }

    if (max_samples < 10 || max_samples > 10000) {
        return SS_CONFIG_INVALID_PERFORMANCE;
    }

    ss_runtime_config.perf_sample_interval = sample_interval;
    ss_runtime_config.perf_max_samples = max_samples;

    return SS_CONFIG_OK;
}

// Validate current configuration
SsConfigResult ss_config_validate(void) {
    // Task validation
    if (ss_runtime_config.max_tasks == 0 || ss_runtime_config.max_tasks > 256) {
        return SS_CONFIG_INVALID_TASKS;
    }

    if (ss_runtime_config.max_task_pri == 0 || ss_runtime_config.max_task_pri > 32) {
        return SS_CONFIG_INVALID_PRIORITY;
    }

    // Memory validation
    if (ss_runtime_config.memory_block_size < 256 || ss_runtime_config.memory_block_size > (1024 * 1024)) {
        return SS_CONFIG_INVALID_MEMORY;
    }

    if (ss_runtime_config.memory_blocks == 0 || ss_runtime_config.memory_blocks > 8192) {
        return SS_CONFIG_INVALID_MEMORY;
    }

    // Performance validation
    if (ss_runtime_config.perf_sample_interval < 10 || ss_runtime_config.perf_sample_interval > 100000) {
        return SS_CONFIG_INVALID_PERFORMANCE;
    }

    if (ss_runtime_config.perf_max_samples < 10 || ss_runtime_config.perf_max_samples > 10000) {
        return SS_CONFIG_INVALID_PERFORMANCE;
    }

    // Graphics validation
    if (ss_runtime_config.max_layers == 0 || ss_runtime_config.max_layers > 1024) {
        return SS_CONFIG_INVALID_GRAPHICS;
    }

    return SS_CONFIG_OK;
}

// Get configuration error string
const char* ss_config_get_error_string(SsConfigResult result) {
    switch (result) {
        case SS_CONFIG_OK:
            return "Configuration is valid";
        case SS_CONFIG_INVALID_TASKS:
            return "Invalid task configuration (must be 1-256)";
        case SS_CONFIG_INVALID_PRIORITY:
            return "Invalid priority configuration (must be 1-32)";
        case SS_CONFIG_INVALID_MEMORY:
            return "Invalid memory configuration";
        case SS_CONFIG_INVALID_PERFORMANCE:
            return "Invalid performance configuration";
        case SS_CONFIG_INVALID_GRAPHICS:
            return "Invalid graphics configuration";
        default:
            return "Unknown configuration error";
    }
}