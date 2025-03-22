#pragma once

#include <stdint.h>

extern const int SW;
extern const int SH;    
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
// extern char __appram_start[];
// extern char __appram_end[];
// extern char __appram_size[];
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
extern const volatile uint32_t ss_save_data_base;

// defined in kernel.c
extern volatile uint8_t* mfp;

void ss_wait_for_vsync();

int ss_handle_keys();

struct KeyBuffer {
    int data[32];
    int idxr;
    int idxw;
    int len;
};

extern struct KeyBuffer ss_kb;
