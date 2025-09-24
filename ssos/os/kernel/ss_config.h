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

// Layer system dimensions (for fast map optimization)
#define SS_CONFIG_LAYER_WIDTH             SS_CONFIG_DISPLAY_WIDTH
#define SS_CONFIG_LAYER_HEIGHT            SS_CONFIG_DISPLAY_HEIGHT

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
#ifndef SS_CONFIG_ENABLE_LAYER
#define SS_CONFIG_ENABLE_LAYER            1
#endif

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
#define SS_CONFIG_PERF_MAX_METRICS        10    // Maximum number of performance metrics

// Compatibility macros for existing code
// Note: Due to C language constraints with struct arrays, most system limits
// must remain compile-time constants. Runtime configuration is not feasible
// for limits that affect struct layouts (arrays) as this would require
// variable-length arrays which are not supported in standard C.

// Task system constants - must be compile-time for TCB and queue arrays
#define MAX_TASKS               SS_CONFIG_MAX_TASKS
#define MAX_TASK_PRI            SS_CONFIG_MAX_TASK_PRI
#define TASK_STACK_SIZE         SS_CONFIG_TASK_STACK_SIZE

// Memory system constants - must be compile-time for free_blocks array
#define MEM_FREE_BLOCKS         SS_CONFIG_MEMORY_BLOCKS
#define MEM_ALIGN_4K            SS_CONFIG_MEMORY_BLOCK_SIZE

// Hardware timing constants - can be runtime configurable (no struct impact)
#define CONTEXT_SWITCH_INTERVAL SS_CONFIG_CONTEXT_SWITCH_INTERVAL
#define KEY_BUFFER_SIZE         SS_CONFIG_KEY_BUFFER_SIZE

// Performance monitoring constants - can be runtime configurable (no struct impact)
#define SS_PERF_MAX_SAMPLES     SS_CONFIG_PERF_MAX_SAMPLES
#define SS_PERF_SAMPLE_INTERVAL SS_CONFIG_PERF_SAMPLE_INTERVAL
#define SS_PERF_MAX_METRICS     SS_CONFIG_PERF_MAX_METRICS

// Graphics constants - must be compile-time for layer arrays
#define MAX_LAYERS              SS_CONFIG_MAX_LAYERS

// Hardware constants
#define MFP_ADDRESS             SS_CONFIG_MFP_ADDRESS
#define VSYNC_BIT               SS_CONFIG_VSYNC_BIT
#define ESC_SCANCODE            SS_CONFIG_ESC_SCANCODE