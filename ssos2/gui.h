#pragma once
#include <stdint.h>

/* ============================================================
 * SSOS2 - Responsive GUI for X68000
 *
 * Architecture:
 *   1. Back buffer in main RAM — fast drawing (no CRTC contention)
 *   2. Dirty region tracking — only transfer changed pixels to VRAM
 *   3. V-sync synchronized flips — no tearing
 *   4. Cooperative multitasking — background tasks update windows
 *
 * Memory layout:
 *   VRAM: 0xC00000, 1024 words/line, pixel = 1 word (color in low byte)
 *   Back buffer: main RAM, 768×512 words (16-bit/pixel)
 *   CGROM: 0xF3A800, 8×16 ANK font bitmaps
 * ============================================================ */

/* --- Hardware addresses --- */
#define VRAM_BASE ((volatile uint16_t*)0xC00000)
#define CGROM_BASE ((const uint8_t*)0xF3A800)
#define MFP_BASE ((volatile uint8_t*)0xE88000)

/* --- Screen dimensions --- */
#define SCREEN_W 512
#define SCREEN_H 512
#define VRAM_W 512 /* VRAM stride: 512 words/line (512x512 layout) */

/* --- Color values (RGB555I: GGGGG RRRRR BBBBB I) --- */
#define RGB(r, g, b) ((((g)&0x1F) << 11) | (((r)&0x1F) << 6) | (((b)&0x1F) << 1))

#define COL_BLACK   RGB(0, 0, 0)
#define COL_BLUE    RGB(0, 0, 20)
#define COL_RED     RGB(20, 0, 0)
#define COL_MAGENTA RGB(20, 0, 20)
#define COL_GREEN   RGB(0, 20, 0)
#define COL_CYAN    RGB(0, 20, 20)
#define COL_YELLOW  RGB(25, 25, 0)
#define COL_WHITE   0xFFFF
#define COL_LGRAY   RGB(24, 24, 24)
#define COL_DGRAY   RGB(12, 12, 12)
#define COL_DESKTOP RGB(0, 10, 15)
#define COL_TITLE_HI RGB(0, 15, 25)
#define COL_ORANGE  RGB(31, 16, 0)
#define COL_TEAL    RGB(0, 20, 20)
#define COL_PURPLE  RGB(15, 0, 15)

/* --- Window (drawn to back buffer) --- */
typedef struct {
    int x, y, w, h;
    uint16_t border_color;
    uint16_t title_bg;
    uint16_t title_fg;
    uint16_t body_bg;
    const char* title;
} Window;

/* --- Application window (links a Window to task data) --- */
#define MAX_WINDOWS 4
#define TITLEBAR_H 17
#define SHADOW_OFFS 4

typedef struct {
    Window win;
    char line1[48]; /* first line of body text */
    char line2[48]; /* second line of body text */
    int dirty;      /* content changed, needs redraw */
} AppWin;

/* --- Initialization & palette --- */
void gui_init(void);
void gui_set_palette(void);

/* --- V-sync --- */
void gui_wait_vsync(void);

/* --- Back-buffer drawing --- */
void gui_clear(uint16_t color);
void gui_fill_rect(int x, int y, int w, int h, uint16_t color);
void gui_draw_rect(int x, int y, int w, int h, uint16_t color);
void gui_draw_dotted_rect_xor(int x, int y, int w, int h, int phase);
void gui_draw_cursor_xor(int x, int y);
void gui_draw_app_windows(void);
void gui_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg);
void gui_draw_text(int x, int y, const char* str, uint16_t fg, uint16_t bg);
void gui_draw_window(const Window* win);
void gui_draw_cursor(int x, int y);

/* --- Multi-window helpers --- */
int gui_register_window(const Window* w);
void gui_draw_all_windows(void);
void gui_mark_window_dirty(int idx);
int gui_hit_test_window(int mx, int my);
void gui_bring_to_top(int idx);

/* --- VRAM transfer --- */
void gui_flip_rect(int x, int y, int w, int h);
void gui_flip_full(void);

/* --- Dirty region tracking --- */
void dirty_reset(void);
void dirty_include(int x, int y, int w, int h);
void dirty_include_window(const Window* w);
void dirty_include_cursor(int cx, int cy);
int dirty_is_empty(void);
void gui_flip_dirty_region(void);

/* --- Global window table (defined in gui.c) --- */
extern AppWin g_wins[MAX_WINDOWS];
extern int g_num_wins;

/* --- Global tick counter (incremented by Timer D ISR) --- */
extern volatile uint32_t ssos2_tick;

/* --- Assembly functions (timer.s) --- */
extern void get_mouse_stat(int16_t* x, int16_t* y, int16_t* buttons);
extern void timerd_isr(void);

/* --- Timer setup/teardown (main.c) --- */
void timer_init(void);
void timer_cleanup(void);

// --- Mouse function (timer.s) ---
// Reads mouse delta via IOCS _MS_GETDT.
// button bits: 0=right pressed, 1=left pressed (0=pressed, 1=released)
extern void get_mouse_delta(int16_t* dx, int16_t* dy, int16_t* buttons);

/* --- DMAC (HD63450) functions --- */
void dma_init(void);
void dma_transfer(const void* src, void* dst, uint32_t count, uint8_t size);
void dma_fill(uint16_t color, void* dst, uint32_t count);
void dma_fill_block(uint16_t color, void* dst, uint16_t w, uint16_t h, uint16_t stride);
void dma_transfer_block(const void* src, void* dst, uint16_t w, uint16_t h, uint16_t src_stride, uint16_t dst_stride);
void dma_copy_words(const void* src, void* dst, uint32_t word_count);
void dma_copy_rect_words(const void* src, void* dst, uint16_t w, uint16_t h, uint16_t src_stride, uint16_t dst_stride);
