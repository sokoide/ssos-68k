#include "ssos_test.h"

#ifdef NATIVE_TEST
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#define SS_CONFIG_VRAM_WIDTH  1024
#define SS_CONFIG_VRAM_HEIGHT 1024

// Mock SSOS types for native testing
typedef void (*FUNCPTR)(int16_t, void*);
typedef enum { TA_HLNG = 1, TA_USERBUF = 0x20 } TaskAttr;
typedef struct {
    void* exinf; // extended info, not used
    uint16_t task_attr;
    FUNCPTR task;
    int8_t task_pri;
    int32_t stack_size;
    void* stack;
} TaskInfo;

// Task manager constants and types
#define MAX_TASKS 16
#define MAX_TASK_PRI 16
#define TASK_STACK_SIZE 4096

// Error codes (matching kernel definitions)
#define E_OK (0)
#define E_PAR (-17)
#define E_LIMIT (-34)
#define E_RSATR (-11)

typedef enum { TS_NONEXIST = 1, TS_READY = 2, TS_WAIT = 4, TS_DORMANT = 8 } TaskState;

typedef enum {
    TWFCT_NON = 0,
    TWFCT_DLY = 1, // waited by sk_dly_tsk
    TWFCT_SLP = 2, // waited by sk_slp_tsk
    TWFCT_FLG = 3, // waited by sk_wait_flag
    TWFCT_SEM = 4, // waited by sk_wait_semaphore
} TaskWaitFactor;

typedef struct _task_control_block {
    void* context;

    // task queue pointers
    struct _task_control_block* prev;
    struct _task_control_block* next;

    // task info
    TaskState state;
    FUNCPTR task_addr;
    int8_t task_pri;
    uint8_t* stack_addr;
    int32_t stack_size;
    int32_t wakeup_count;

    // wait info
    TaskWaitFactor wait_factor;
    uint32_t wait_time;
    uint32_t* wait_err;

    // event flag wait info
    uint32_t wait_pattern;
    uint32_t wait_mode;
    uint32_t* p_flag_pattern; // flag pattern when wait canceled

    // semaphore wait info
    int32_t wait_semaphore;
} TaskControlBlock;

TaskControlBlock tcb_table[MAX_TASKS];  // Will be initialized by reset_scheduler_state()
TaskControlBlock* ready_queue[MAX_TASK_PRI];  // Don't initialize here - will be done in reset
TaskControlBlock* scheduled_task = NULL;
uint16_t main_task_id = 0;

// Memory manager structure
typedef struct {
    int num_free_blocks;
    struct {
        uint32_t addr;
        uint32_t sz;
    } free_blocks[100];
} MemoryManager;

MemoryManager ss_mem_mgr = {0};

// Forward declaration
uint32_t ss_mem_free_bytes(void);
void ss_scheduler(void);

// Mock memory functions - Return real host memory addresses for native testing
uint32_t ss_mem_alloc(uint32_t size) {
    if (size == 0) {
        return 0;  // Return 0 for zero-size allocation
    }

    // Check if we have memory available in our simulated memory manager
    uint32_t available = ss_mem_free_bytes();
    if (size > available) {
        return 0;  // Out of memory
    }

    // For native testing, always use real host memory that can be dereferenced
    void* real_mem = malloc(size);
    if (!real_mem) {
        return 0;  // malloc failed
    }

    // Update memory manager state for consistency with tests
    if (ss_mem_mgr.num_free_blocks > 0) {
        if (ss_mem_mgr.free_blocks[0].sz >= size) {
            if (ss_mem_mgr.free_blocks[0].sz == size) {
                // Complete consumption - remove free block
                ss_mem_mgr.num_free_blocks = 0;
            } else {
                // Partial allocation - update free block
                ss_mem_mgr.free_blocks[0].addr += size;
                ss_mem_mgr.free_blocks[0].sz -= size;
            }
        }
    }

    return (uint32_t)(uintptr_t)real_mem;
}

uint32_t ss_mem_alloc4k(uint32_t size) {
    if (size == 0) {
        return 0;
    }
    size = (size + 4095) & ~4095;  // Round up to 4K

    // For native testing, allocate 4K-aligned memory
    void* real_mem;
    if (posix_memalign(&real_mem, 4096, size) != 0) {
        return 0;  // allocation failed
    }
    return (uint32_t)(uintptr_t)real_mem;
}

void ss_mem_init(void) {
    ss_mem_mgr.num_free_blocks = 1;
    ss_mem_mgr.free_blocks[0].addr = 0x100000;
    ss_mem_mgr.free_blocks[0].sz = 0x100000;  // 1MB for testing
}

uint32_t ss_mem_total_bytes(void) {
    return 0x100000;  // 1MB total for testing
}

uint32_t ss_mem_free_bytes(void) {
    uint32_t free_bytes = 0;
    for (int i = 0; i < ss_mem_mgr.num_free_blocks; i++) {
        free_bytes += ss_mem_mgr.free_blocks[i].sz;
    }
    return free_bytes;
}

// Initialize scheduler state
void reset_scheduler_state(void) {
    // Initialize TCB table - make sure ALL slots are properly reset
    for (int i = 0; i < MAX_TASKS; i++) {
        // Zero out entire structure first to ensure clean state
        memset(&tcb_table[i], 0, sizeof(TaskControlBlock));
        // Then set the proper non-existent state
        tcb_table[i].state = TS_NONEXIST;
        tcb_table[i].task_pri = 0;
        tcb_table[i].next = NULL;
        tcb_table[i].prev = NULL;
        tcb_table[i].task_addr = NULL;
        tcb_table[i].stack_addr = NULL;
        tcb_table[i].stack_size = 0;
        tcb_table[i].context = NULL;
        tcb_table[i].wait_factor = TWFCT_NON;
        tcb_table[i].wakeup_count = 0;
        tcb_table[i].wait_time = 0;
        tcb_table[i].wait_err = NULL;
        tcb_table[i].wait_pattern = 0;
        tcb_table[i].wait_mode = 0;
        tcb_table[i].p_flag_pattern = NULL;
        tcb_table[i].wait_semaphore = 0;
    }

    // Initialize ready queues
    for (int i = 0; i < MAX_TASK_PRI; i++) {
        ready_queue[i] = NULL;
    }

    scheduled_task = NULL;
    main_task_id = 0;
}

// Mock task functions
uint16_t ss_create_task(const TaskInfo* info) {
    if (!info || !info->task || info->task_pri < 1 || info->task_pri > MAX_TASK_PRI) {
        return E_PAR;  // Invalid parameters - matches kernel behavior
    }

    // Find available task slot
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tcb_table[i].state == TS_NONEXIST) {
            // Initialize TCB
            tcb_table[i].state = TS_DORMANT;
            tcb_table[i].task_addr = info->task;
            tcb_table[i].task_pri = info->task_pri;
            tcb_table[i].stack_size = info->stack_size;

            // Handle stack allocation based on task attributes
            if (info->task_attr & TA_USERBUF) {
                // User provided their own stack buffer
                tcb_table[i].stack_addr = (uint8_t*)info->stack;
                if (tcb_table[i].stack_addr == NULL) {
                    // Reset the slot if user didn't provide stack
                    tcb_table[i].state = TS_NONEXIST;
                    tcb_table[i].task_addr = NULL;
                    tcb_table[i].task_pri = 0;
                    tcb_table[i].stack_size = 0;
                    return E_PAR;  // Invalid parameters
                }
            } else {
                // System allocates stack memory
                uint32_t stack_addr = ss_mem_alloc(info->stack_size);
                if (stack_addr == 0) {
                    // Reset the slot if memory allocation fails
                    tcb_table[i].state = TS_NONEXIST;
                    tcb_table[i].task_addr = NULL;
                    tcb_table[i].task_pri = 0;
                    tcb_table[i].stack_size = 0;
                    return E_LIMIT;  // Stack allocation failed (out of memory)
                }
                tcb_table[i].stack_addr = (uint8_t*)(uintptr_t)stack_addr;
            }
            tcb_table[i].next = NULL;
            tcb_table[i].prev = NULL;
            tcb_table[i].context = NULL;

            return i + 1;  // Return 1-based task ID
        }
    }

    return E_LIMIT;  // No available slots
}

int ss_start_task(int task_id, int arg) {
    if (task_id < 1 || task_id > MAX_TASKS) {
        return -1;  // Invalid task ID
    }

    TaskControlBlock* tcb = &tcb_table[task_id - 1];
    if (tcb->state != TS_DORMANT) {
        return -1;  // Task not in dormant state
    }

    // Move task to ready state
    tcb->state = TS_READY;

    // Add to ready queue based on priority
    int pri = tcb->task_pri;
    if (pri >= 1 && pri <= MAX_TASK_PRI) {
        // Convert 1-based priority to 0-based array index
        // Priority 1 (highest) goes to ready_queue[0]
        // Priority 2 goes to ready_queue[1]
        // etc.
        int queue_index = pri - 1;
        tcb->next = ready_queue[queue_index];
        ready_queue[queue_index] = tcb;
    }

    // Call scheduler to update scheduled_task
    ss_scheduler();

    return 0;  // Success
}

// Mock scheduler function - selects highest priority ready task
void ss_scheduler(void) {
    scheduled_task = NULL;

    // Find highest priority ready task (lowest index = highest priority)
    for (int i = 0; i < MAX_TASK_PRI; i++) {
        if (ready_queue[i] != NULL) {
            scheduled_task = ready_queue[i];
            break;
        }
    }
}

// Task manager structures already defined above

// Mock layer system - matching real layer.h interface
#include "ss_config.h"  // For MAX_LAYERS

enum LayerAttr {
    LAYER_ATTR_USED = 0x01,
    LAYER_ATTR_VISIBLE = 0x02,
};

typedef struct {
    uint16_t x, y, z;
    uint16_t w, h;
    uint16_t attr;
    uint8_t* vram;
    // Dirty rectangle tracking for performance
    uint16_t dirty_x, dirty_y, dirty_w, dirty_h;
    uint8_t needs_redraw;
} Layer;

typedef struct {
    int16_t topLayerIdx;
    Layer* zLayers[MAX_LAYERS]; // z-order sorted layer pointers
    Layer layers[MAX_LAYERS];   // layer data
    uint8_t* map;
} LayerMgr;

static LayerMgr global_layer_mgr;
LayerMgr* ss_layer_mgr = &global_layer_mgr;

Layer* ss_layer_get(void) {
    // Find an available layer in the manager's layer array
    for (int i = 0; i < MAX_LAYERS; i++) {
        if (!(ss_layer_mgr->layers[i].attr & LAYER_ATTR_USED)) {
            // Initialize layer
            ss_layer_mgr->layers[i].attr = LAYER_ATTR_USED | LAYER_ATTR_VISIBLE;
            ss_layer_mgr->layers[i].vram = NULL;
            ss_layer_mgr->layers[i].x = ss_layer_mgr->layers[i].y = 0;
            ss_layer_mgr->layers[i].w = ss_layer_mgr->layers[i].h = 0;
            ss_layer_mgr->layers[i].z = ss_layer_mgr->topLayerIdx++;
            ss_layer_mgr->layers[i].dirty_x = ss_layer_mgr->layers[i].dirty_y = 0;
            ss_layer_mgr->layers[i].dirty_w = ss_layer_mgr->layers[i].dirty_h = 0;
            ss_layer_mgr->layers[i].needs_redraw = 1;

            // Add to z-order array
            ss_layer_mgr->zLayers[ss_layer_mgr->layers[i].z] = &ss_layer_mgr->layers[i];

            return &ss_layer_mgr->layers[i];
        }
    }
    return NULL;
}

void ss_layer_set(Layer* layer, uint8_t* vram, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (layer) {
        layer->vram = vram;
        layer->x = x; layer->y = y;
        layer->w = w; layer->h = h;
        // When VRAM is set, mark layer as needing redraw
        layer->needs_redraw = 1;
        layer->dirty_x = 0;
        layer->dirty_y = 0;
        layer->dirty_w = w;
        layer->dirty_h = h;
    }
}

void ss_layer_move(Layer* layer, int x, int y) {
    if (layer) {
        layer->x = x; layer->y = y;
    }
}

void ss_layer_slide(Layer* layer, int height) {
    if (layer) {
        layer->z = height;
    }
}

void ss_layer_init(void) {
    // Initialize layer manager
    ss_layer_mgr->topLayerIdx = 0;
    for (int i = 0; i < MAX_LAYERS; i++) {
        ss_layer_mgr->zLayers[i] = NULL;
        ss_layer_mgr->layers[i].attr = 0;  // Mark as unused
    }
    ss_layer_mgr->map = (uint8_t*)(uintptr_t)ss_mem_alloc(1024 * 1024 / 64);  // Mock allocation
}

// This function is not used in the current tests, but keeping for compatibility
// Layer* ss_layer_get_by_id(int id) {
//     if (id >= 0 && id < MAX_LAYERS && (ss_layer_mgr->layers[id].attr & LAYER_ATTR_USED)) {
//         return &ss_layer_mgr->layers[id];
//     }
//     return NULL;
// }

void ss_layer_set_z(Layer* layer, uint16_t z) {
    if (layer && ss_layer_mgr) {
        // Remove from old z-position
        if (layer->z >= 0 && layer->z < MAX_LAYERS) {
            ss_layer_mgr->zLayers[layer->z] = NULL;
        }

        // Set new z-position
        layer->z = z;
        if (z < MAX_LAYERS) {
            ss_layer_mgr->zLayers[z] = layer;
            // Update top layer index
            if (z >= ss_layer_mgr->topLayerIdx) {
                ss_layer_mgr->topLayerIdx = z + 1;
            }
        }
    }
}

void ss_layer_mark_dirty(Layer* layer, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    if (layer) {
        layer->needs_redraw = 1;
        layer->dirty_x = x;
        layer->dirty_y = y;
        layer->dirty_w = w;
        layer->dirty_h = h;
    }
}

void ss_layer_mark_clean(Layer* layer) {
    if (layer) {
        layer->needs_redraw = 0;  // Clear the redraw flag
    }
}

void ss_layer_invalidate(Layer* layer) {
    if (layer) {
        layer->needs_redraw = 1;
        layer->dirty_x = 0;
        layer->dirty_y = 0;
        layer->dirty_w = layer->w;
        layer->dirty_h = layer->h;
    }
}

// Mock error handling system
typedef enum {
    SS_SUCCESS = 0,
    SS_ERROR_NULL_PTR = 1,
    SS_ERROR_INVALID_PARAM = 2,
    SS_ERROR_OUT_OF_MEMORY = 3,
    SS_ERROR_SYSTEM_ERROR = 4
} SsError;

typedef enum {
    SS_SEVERITY_INFO = 0,
    SS_SEVERITY_WARNING = 1,
    SS_SEVERITY_ERROR = 2,
    SS_SEVERITY_CRITICAL = 3
} SsSeverity;

typedef struct {
    SsError error_code;
    SsSeverity severity;
    const char* function_name;
    const char* file_name;
    uint32_t line_number;
    uint32_t timestamp;
    const char* description;
} SsErrorContext;

SsErrorContext ss_last_error = {SS_SUCCESS, SS_SEVERITY_INFO, NULL, NULL, 0, 0, NULL};
uint32_t ss_error_count = 0;

void ss_set_error(SsError code, SsSeverity severity, const char* function_name,
                  const char* file_name, uint32_t line_number, const char* description) {
    ss_last_error.error_code = code;
    ss_last_error.severity = severity;
    ss_last_error.function_name = function_name;
    ss_last_error.file_name = file_name;
    ss_last_error.line_number = line_number;
    ss_last_error.description = description;
    ss_last_error.timestamp = ss_error_count++;  // Use counter as timestamp
}

SsError ss_get_last_error(void) {
    return ss_last_error.error_code;
}

const char* ss_error_to_string(SsError code) {
    switch (code) {
        case SS_SUCCESS: return "Success";
        case SS_ERROR_NULL_PTR: return "Null pointer";
        case SS_ERROR_INVALID_PARAM: return "Invalid parameter";
        case SS_ERROR_OUT_OF_MEMORY: return "Out of memory";
        case SS_ERROR_SYSTEM_ERROR: return "System error";
        default: return "Unknown error";
    }
}

#else
#include "memory.h"
#include "ss_config.h"
#endif

// Memory layout symbols for LOCAL_MODE testing
void* local_ssos_memory_base = (void*)0x100000;  // 1MB base for testing
uint32_t local_ssos_memory_size = 0x100000;      // 1MB size for testing
uint32_t local_text_size = 0x10000;              // 64KB text section
uint32_t local_data_size = 0x1000;               // 4KB data section
uint32_t local_bss_size = 0x1000;                // 4KB BSS section

// VRAM base address for testing
uint8_t test_vram_buffer[SS_CONFIG_VRAM_WIDTH * SS_CONFIG_VRAM_HEIGHT];
uint8_t* vram_start = test_vram_buffer;

// DMA transfer info structure (mock)
typedef struct {
    uint32_t src;
    uint32_t dst;
    uint32_t size;
} xfr_info_t;

xfr_info_t xfr_inf;

// Mock DMA functions for testing
void dma_clear(void) {
    // Mock implementation - just clear the transfer info
    xfr_inf.src = 0;
    xfr_inf.dst = 0;
    xfr_inf.size = 0;
}

void dma_init(uint32_t src, uint32_t dst, uint32_t size) {
    // Mock implementation - store transfer parameters
    xfr_inf.src = src;
    xfr_inf.dst = dst;
    xfr_inf.size = size;
}

void dma_start(void) {
    // Mock implementation - for testing, just do a memory copy
    if (xfr_inf.size > 0) {
        // Simple memcpy for testing
        uint8_t* src_ptr = (uint8_t*)(uintptr_t)xfr_inf.src;
        uint8_t* dst_ptr = (uint8_t*)(uintptr_t)xfr_inf.dst;
        for (uint32_t i = 0; i < xfr_inf.size; i++) {
            dst_ptr[i] = src_ptr[i];
        }
    }
}

void dma_wait_completion(void) {
    // Mock implementation - no waiting needed in test environment
}

// Additional symbols needed by kernel
char local_info[256] = "Test Environment";

// Mock interrupt control functions (needed by task_manager.c)
void disable_interrupts(void) {
    // Mock implementation - do nothing in test environment
}

void enable_interrupts(void) {
    // Mock implementation - do nothing in test environment
}

// Mock timer counter (needed by ss_errors.c and ss_perf.c)
const volatile uint32_t ss_timerd_counter = 0;

// Mock main application function
void ssosmain(void) {
    // Mock implementation for testing - do nothing
    // This prevents the task manager from trying to start the real main app
}