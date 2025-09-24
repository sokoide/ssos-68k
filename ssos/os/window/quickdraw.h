#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>  // for size_t

// 768x512, 16 colour (4bpp) front buffer layout - Standard 8-bit VRAM
// Match Layer system's VRAM usage (2 bytes per 2 pixels, 16-bit VRAM with 4 unused bits)
#define QD_SCREEN_WIDTH   768
#define QD_SCREEN_HEIGHT  512
#define QD_PIXELS_PER_ROW (QD_SCREEN_WIDTH)
// X68000 16-color VRAM: 2 bytes per pixel (16-bit), with 1024-pixel VRAM stride
#define QD_BYTES_PER_ROW  (QD_SCREEN_WIDTH * 2)    // 1536 bytes per row (display data)
#define QD_VRAM_STRIDE    (SS_CONFIG_VRAM_WIDTH * 2) // 2048 bytes per VRAM row (full stride)
#define QD_VRAM_BYTES     (QD_BYTES_PER_ROW * QD_SCREEN_HEIGHT)

enum {
    QD_COLOR_BLACK = 0,
    QD_COLOR_BLUE,
    QD_COLOR_GREEN,
    QD_COLOR_CYAN,
    QD_COLOR_RED,
    QD_COLOR_MAGENTA,
    QD_COLOR_BROWN,
    QD_COLOR_WHITE,
    QD_COLOR_GRAY,
    QD_COLOR_LTBLUE,
    QD_COLOR_LTGREEN,
    QD_COLOR_LTCYAN,
    QD_COLOR_LTRED,
    QD_COLOR_LTMAGENTA,
    QD_COLOR_YELLOW,
    QD_COLOR_BRIGHT_WHITE,
};

typedef struct {
    int16_t x;
    int16_t y;
    uint16_t width;
    uint16_t height;
} QD_Rect;

void qd_init(void);
bool qd_is_initialized(void);
uint16_t qd_get_screen_width(void);
uint16_t qd_get_screen_height(void);

void qd_set_vram_buffer(uint8_t* buffer);
uint8_t* qd_get_vram_buffer(void);

void qd_set_clip_rect(int16_t x, int16_t y, uint16_t width, uint16_t height);
const QD_Rect* qd_get_clip_rect(void);
bool qd_clip_point(int16_t x, int16_t y);
bool qd_clip_rect(int16_t* x, int16_t* y, uint16_t* width, uint16_t* height);

void qd_clear_screen(uint8_t color);
void qd_clear_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);

void qd_set_pixel(int16_t x, int16_t y, uint8_t color);
uint8_t qd_get_pixel(int16_t x, int16_t y);

void qd_draw_hline(int16_t x, int16_t y, uint16_t length, uint8_t color);
void qd_draw_vline(int16_t x, int16_t y, uint16_t length, uint8_t color);
void qd_draw_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);
void qd_fill_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);
void qd_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);
void qd_set_font_bitmap(const uint8_t* font_bitmap, uint16_t glyph_width, uint16_t glyph_height);
uint16_t qd_get_font_width(void);
uint16_t qd_get_font_height(void);
uint16_t qd_measure_text(const char* text);
void qd_draw_char(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color, bool opaque_bg);
void qd_draw_text(int16_t x, int16_t y, const char* text, uint8_t fg_color, uint8_t bg_color, bool opaque_bg);

// DMA optimization constants (matching Layer system)
#define QD_ADAPTIVE_DMA_THRESHOLD_MIN 4
#define QD_ADAPTIVE_DMA_THRESHOLD_MAX 12
#define QD_ADAPTIVE_DMA_THRESHOLD_DEFAULT 8

// DMA optimization functions
void qd_update_performance_metrics(void);
void qd_memory_copy(void* dst, const void* src, size_t count);
