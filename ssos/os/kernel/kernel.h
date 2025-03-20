#pragma once

#include <stdint.h>

extern const int WIDTH;
extern const int HEIGHT;
extern const uint16_t color_fg;
extern const uint16_t color_bg;
extern const uint16_t color_tb;

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

// local only: defined in local/main.c
extern char local_info[256];

extern volatile uint8_t* mfp;

void ss_wait_for_vsync();

int ss_handle_keys();

struct KeyBuffer {
    int data[32];
    int idxr;
    int idxw;
    int len;
};

extern struct KeyBuffer kb;
