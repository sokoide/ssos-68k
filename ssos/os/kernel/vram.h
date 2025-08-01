#pragma once

#include <stdint.h>
#include <stddef.h>

// Macro taken from https://sourceforge.net/projects/x68000-doom/
// Merge 3x 8bit values to 15bit + intensity in GRB format
// Keep only the 5 msb of each value
#define rgb888_2grb(r, g, b, i)                                                \
    (((b & 0xF8) >> 2) | ((g & 0xF8) << 8) | ((r & 0xF8) << 3) | i)

// Merge 3x 8bit values to 15bit + intensity in RGB format
// Keep only the 5 msb of each value
#define rgb888_2rgb(r, g, b, i)                                                \
    (((r & 0xF8) << 8) | ((g & 0xF8) << 3) | ((b & 0xF8) >> 2) | i)

typedef struct {
    short sc0_x_reg;
    short sc0_y_reg;
    short sc1_x_reg;
    short sc1_y_reg;
    short sc2_x_reg;
    short sc2_y_reg;
    short sc3_x_reg;
    short sc3_y_reg;
} CRTC_REG;

void ss_clear_vram_fast();
void ss_wait_for_clear_vram_completion();

void ss_fill_rect_v(uint8_t* offscreen, uint16_t sw, uint16_t sh,
                    uint16_t color, int x0, int y0, int x1, int y1);

void ss_draw_rect_v(uint8_t* offscreen, uint16_t sw, uint16_t sh,
                    uint16_t color, int x0, int y0, int x1, int y1);

void ss_put_char_v(uint8_t* offscreen, uint16_t sw, uint16_t sh,
                   uint16_t fg_color, uint16_t bg_color, int x, int y, char c);

void ss_print_v(uint8_t* offscreen, uint16_t sw, uint16_t sh, uint16_t fg_color,
                uint16_t bg_color, int x, int y, char* str);

// Smart text functions that only redraw when text changes
int ss_print_v_smart(uint8_t* offscreen, uint16_t sw, uint16_t sh, uint16_t fg_color,
                     uint16_t bg_color, int x, int y, char* str, char* prev_str);

void ss_init_palette();

// Utility functions
int mystrlen(char* str);

// Memory alignment helpers for 68000 optimization
static inline int ss_is_aligned_32(void* ptr) {
    return ((uintptr_t)ptr & 3) == 0;
}

static inline int ss_is_aligned_16(void* ptr) {
    return ((uintptr_t)ptr & 1) == 0;
}

// Fast memory copy functions optimized for 68000
void ss_memcpy_32(uint32_t* dst, const uint32_t* src, size_t count);
void ss_memset_32(uint32_t* dst, uint32_t value, size_t count);

// Optimized rectangle fill using word-alignment
void ss_fill_rect_v_fast(uint8_t* offscreen, uint16_t w, uint16_t h, 
                         uint16_t color, int x0, int y0, int x1, int y1);

// globals
extern CRTC_REG scroll_data;

// IO ports
extern volatile CRTC_REG* crtc;
extern volatile uint16_t* crtc_execution_port;

// vram address
extern uint16_t* vram_start;
extern uint16_t* vram_end;
