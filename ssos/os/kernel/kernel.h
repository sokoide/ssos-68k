#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ss_config.h"
#include "ss_errors.h"

// *** constants ***
// All constants now defined in ss_config.h for better maintainability
// These compatibility macros are provided for existing code

// *** VRAM ***
// Extern declarations for constants defined in kernel.c
extern const int VRAMWIDTH;
extern const int VRAMHEIGHT;
extern const int WIDTH;
extern const int HEIGHT;
extern const uint16_t color_fg;
extern const uint16_t color_bg;
extern const uint16_t color_tb;

// ***** disk boot only *****
// defined in linker.ld
extern char __text_start[];
extern char __text_end[];
extern char __text_size[];
extern char __data_start[];
extern char __data_end[];
extern char __data_size[];
extern char __bss_start[];
extern char __bss_end[];
extern char __bss_size[];
extern char __ssosram_start[];
extern char __ssosram_end[];
extern char __ssosram_size[];

// ***** local only *****
// defined in local/main.c
extern char local_info[256];
extern void* local_ssos_memory_base;
extern uint32_t local_ssos_memory_size;
extern uint32_t local_text_size, local_data_size, local_bss_size;

// ***** common *****
// SSOS ports: defined in interrupts.s
extern const volatile uint32_t ss_timera_counter;
extern const volatile uint32_t ss_timerb_counter;
extern const volatile uint32_t ss_timerc_counter;
extern const volatile uint32_t ss_timerd_counter;
extern const volatile uint32_t ss_key_counter;
extern const volatile uint32_t ss_context_switch_counter;
extern const volatile uint32_t ss_save_data_base;

extern void disable_interrupts(void);
extern void enable_interrupts(void);

// defined in kernel.c
extern volatile uint8_t* mfp;

// KEY_BUFFER_SIZE now defined in ss_config.h

void ss_wait_for_vsync();

int ss_handle_keys();
void ss_kb_init();
int ss_kb_read();
bool ss_kb_is_empty();

struct KeyBuffer {
    volatile int data[KEY_BUFFER_SIZE];
    volatile int idxr;
    volatile int idxw;
    volatile int len;
};

extern struct KeyBuffer ss_kb;

#define SS_KB_MOD_SHIFT 0x01
#define SS_KB_MOD_CTRL 0x02
#define SS_KB_MOD_CAPS 0x04

#define X68K_SC_ESC 0x6D

// defined in task_manager.c
typedef enum {
    TS_NONEXIST = 1,
    TS_READY = 2,
    TS_WAIT = 4,
    TS_DORMANT = 8
} TaskState;

typedef enum {
    TWFCT_NON = 0,
    TWFCT_DLY = 1,  // waited by sk_dly_tsk
    TWFCT_SLP = 2,  // waited by sk_slp_tsk
    TWFCT_FLG = 3,  // waited by sk_wait_flag
    TWFCT_SEM = 4,  // waited by sk_wait_semaphore
} TaskWaitFactor;

typedef enum { KS_NONEXIST = 0, KS_EXIST = 1 } KernelState;

typedef void (*FUNCPTR)(int16_t, void*); /* function pointer */

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
    uint32_t* p_flag_pattern;  // flag pattern when wait canceled

    // semaphore wait info
    int32_t wait_semaphore;
} TaskControlBlock;

typedef struct {
    KernelState state;
    uint32_t pattern;
} FlagControlBlock;

typedef struct {
    KernelState state;
    int32_t value;
    int32_t max_value;
} SemaphoreControlBlock;

extern TaskControlBlock tcb_table[];
extern TaskControlBlock* ready_queue[];
extern TaskControlBlock* curr_task;
extern TaskControlBlock* scheduled_task;  // scheduled task which will be
                                          // executed after the curr_task
extern TaskControlBlock* wait_queue;
extern uint32_t dispatch_running;
extern FlagControlBlock fcb_table[];
extern uint32_t global_counter;
