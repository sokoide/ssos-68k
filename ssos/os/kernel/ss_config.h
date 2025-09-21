#pragma once

// SSOS Configuration System
// This file contains all configurable parameters to replace magic numbers

// System Configuration
#define SS_CONFIG_MAX_TASKS               16
#define SS_CONFIG_MAX_TASK_PRI            16
#define SS_CONFIG_TASK_STACK_SIZE         (4 * 1024)

// Memory Configuration
#define SS_CONFIG_MEMORY_BLOCK_SIZE       4096
#define SS_CONFIG_MEMORY_BLOCKS           1024
#define SS_CONFIG_MEMORY_TOTAL_SIZE       (SS_CONFIG_MEMORY_BLOCK_SIZE * SS_CONFIG_MEMORY_BLOCKS)

// Timer Configuration
#define SS_CONFIG_CONTEXT_SWITCH_INTERVAL 16
#define SS_CONFIG_TIMER_FREQUENCY         1000  // Hz

// Keyboard Configuration
#define SS_CONFIG_KEY_BUFFER_SIZE         32

// Graphics Configuration
#define SS_CONFIG_VRAM_WIDTH              1024
#define SS_CONFIG_VRAM_HEIGHT             1024
#define SS_CONFIG_DISPLAY_WIDTH           768
#define SS_CONFIG_DISPLAY_HEIGHT          512

// Color Configuration
#define SS_CONFIG_COLOR_FOREGROUND        15
#define SS_CONFIG_COLOR_BACKGROUND        10
#define SS_CONFIG_COLOR_TASKBAR           14

// Hardware Configuration
#define SS_CONFIG_MFP_ADDRESS             0xe88001
#define SS_CONFIG_VSYNC_BIT               0x10
#define SS_CONFIG_ESC_SCANCODE            0x011b

// Font Configuration
#define SS_CONFIG_FONT_WIDTH              8
#define SS_CONFIG_FONT_HEIGHT             16
#define SS_CONFIG_FONT_BASE_ADDRESS       0xf3a800

// Layer Configuration
#define SS_CONFIG_MAX_LAYERS              256

// DMA Configuration
#define SS_CONFIG_DMA_MAX_TRANSFERS       512

// Debug Configuration
#ifdef SS_DEBUG
#define SS_CONFIG_ENABLE_ASSERTIONS       1
#define SS_CONFIG_ENABLE_ERROR_LOGGING    1
#define SS_CONFIG_ENABLE_PERFORMANCE_MONITORING 1
#else
#define SS_CONFIG_ENABLE_ASSERTIONS       0
#define SS_CONFIG_ENABLE_ERROR_LOGGING    0
#define SS_CONFIG_ENABLE_PERFORMANCE_MONITORING 0
#endif

// Performance Monitoring Configuration
#define SS_CONFIG_PERF_SAMPLE_INTERVAL    1000  // Sample every 1000 timer ticks
#define SS_CONFIG_PERF_MAX_SAMPLES        100   // Keep last 100 samples
#define SS_CONFIG_PERF_MAX_METRICS        7     // Maximum number of performance metrics

// Runtime Configuration System
typedef struct {
    // Task system limits
    uint16_t max_tasks;
    uint16_t max_task_pri;
    uint32_t task_stack_size;

    // Memory system limits
    uint32_t memory_block_size;
    uint32_t memory_blocks;
    uint32_t memory_total_size;

    // Performance monitoring limits
    uint32_t perf_sample_interval;
    uint32_t perf_max_samples;
    uint32_t perf_max_metrics;

    // Graphics system limits
    uint16_t max_layers;

    // Hardware limits
    uint16_t key_buffer_size;
    uint32_t context_switch_interval;
} SsRuntimeConfig;

// Global runtime configuration instance
extern SsRuntimeConfig ss_runtime_config;

// Configuration validation result
typedef enum {
    SS_CONFIG_OK = 0,
    SS_CONFIG_INVALID_TASKS = -1,
    SS_CONFIG_INVALID_PRIORITY = -2,
    SS_CONFIG_INVALID_MEMORY = -3,
    SS_CONFIG_INVALID_PERFORMANCE = -4,
    SS_CONFIG_INVALID_GRAPHICS = -5
} SsConfigResult;

// Runtime configuration functions
SsConfigResult ss_config_init(void);
SsConfigResult ss_config_set_task_limits(uint16_t max_tasks, uint16_t max_priority);
SsConfigResult ss_config_set_memory_limits(uint32_t block_size, uint32_t blocks);
SsConfigResult ss_config_set_performance_limits(uint32_t sample_interval, uint32_t max_samples);
SsConfigResult ss_config_validate(void);

// Compatibility macros for existing code (excluding conflicting ones)
#define MAX_TASKS               ss_runtime_config.max_tasks
#define MAX_TASK_PRI            ss_runtime_config.max_task_pri
#define TASK_STACK_SIZE         ss_runtime_config.task_stack_size
#define MEM_FREE_BLOCKS         ss_runtime_config.memory_blocks
#define MEM_ALIGN_4K            ss_runtime_config.memory_block_size
#define CONTEXT_SWITCH_INTERVAL ss_runtime_config.context_switch_interval
#define KEY_BUFFER_SIZE         ss_runtime_config.key_buffer_size

// Performance monitoring compatibility macros
#define SS_PERF_MAX_SAMPLES     ss_runtime_config.perf_max_samples
#define SS_PERF_SAMPLE_INTERVAL ss_runtime_config.perf_sample_interval
#define SS_PERF_MAX_METRICS     ss_runtime_config.perf_max_metrics

// VRAM and color constants are now defined in kernel.h with proper extern declarations
// to avoid macro conflicts in the linker
#define MFP_ADDRESS             SS_CONFIG_MFP_ADDRESS
#define VSYNC_BIT               SS_CONFIG_VSYNC_BIT
#define ESC_SCANCODE            SS_CONFIG_ESC_SCANCODE
#define MAX_LAYERS              ss_runtime_config.max_layers