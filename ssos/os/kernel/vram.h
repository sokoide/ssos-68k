#pragma once

#include <stdint.h>

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

void ss_clear_vram();
void ss_clear_vram_fast();
void ss_wait_for_clear_vram_completion();

void ss_fill_rect(uint16_t color, int x0, int y0, int x1, int y1);

void ss_put_char(uint16_t fg_color, uint16_t bg_color, int x, int y, char c);
void ss_print(uint16_t fg_color, uint16_t bg_color, int x, int y, char* str);
void ss_init_palette();

// globals
extern CRTC_REG scroll_data;

// IO ports
extern volatile CRTC_REG* crtc;
extern volatile uint16_t* crtc_execution_port;

// vram address
extern volatile uint16_t* vram_start;
extern volatile uint16_t* vram_end;
