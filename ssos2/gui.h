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
 *   Back buffer: main RAM, 768×512 bytes (1 byte/pixel)
 *   CGROM: 0xF3A800, 8×16 ANK font bitmaps
 * ============================================================ */

/* --- Hardware addresses --- */
#define VRAM_BASE ((volatile uint16_t*)0xC00000)
#define CGROM_BASE ((const uint8_t*)0xF3A800)
#define MFP_BASE ((volatile uint8_t*)0xE88000)

/* --- Screen dimensions --- */
#define SCREEN_W 768
#define SCREEN_H 512
#define VRAM_W 1024 /* VRAM width in words (pixels) */

/* --- Color indices (set by gui_set_palette) --- */
#define COL_BLACK 0
#define COL_BLUE 1
#define COL_RED 2
#define COL_MAGENTA 3
#define COL_GREEN 4
#define COL_CYAN 5
#define COL_YELLOW 6
#define COL_WHITE 7
#define COL_LGRAY 8
#define COL_DGRAY 9
#define COL_DESKTOP 10
#define COL_TITLE_HI 11
#define COL_ORANGE 12
#define COL_TEAL 13
#define COL_PURPLE 14

/* --- Window (drawn to back buffer) --- */
typedef struct {
    int x, y, w, h;
    uint8_t border_color;
    uint8_t title_bg;
    uint8_t title_fg;
    uint8_t body_bg;
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
void gui_clear(uint8_t color);
void gui_fill_rect(int x, int y, int w, int h, uint8_t color);
void gui_draw_rect(int x, int y, int w, int h, uint8_t color);
void gui_draw_dotted_rect_xor(int x, int y, int w, int h, int phase);
void gui_draw_cursor_xor(int x, int y);
void gui_draw_app_windows(void);
void gui_draw_char(int x, int y, char c, uint8_t fg, uint8_t bg);
void gui_draw_text(int x, int y, const char* str, uint8_t fg, uint8_t bg);
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

// --- GUI functions (gui.c) ---
void gui_init(void);
void gui_set_palette(void);
void gui_clear(uint8_t color);
void gui_fill_rect(int x, int y, int w, int h, uint8_t color);
void gui_draw_rect(int x, int y, int w, int h, uint8_t color);
void gui_draw_dotted_rect_xor(int x, int y, int w, int h, int phase);
void gui_draw_cursor_xor(int x, int y);
void gui_draw_app_windows(void);
void gui_draw_char(int x, int y, char c, uint8_t fg, uint8_t bg);
void gui_draw_text(int x, int y, const char* str, uint8_t fg, uint8_t bg);
void gui_draw_window(const Window* win);
void gui_flip_rect(int x, int y, int w, int h);
void gui_flip_full(void);
void gui_wait_vsync(void);

// --- Timer functions (main.c + timer.s) ---
void timer_init(void);
void timer_cleanup(void);

// --- Mouse function (timer.s) ---
// Reads mouse delta via IOCS _MS_GETDT.
// button bits: 0=right pressed, 1=left pressed (0=pressed, 1=released)
extern void get_mouse_delta(int16_t* dx, int16_t* dy, int16_t* buttons);
