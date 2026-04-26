/*
 * SSOS3 Standalone — Interactive Windowed Demo for X68000
 * CRT: 320x256 65536-color (crtmod 13)
 * Font: 8x8 ANK via _iocs_fntget
 * Exit: ESC key only
 */

#include "../os/kernel/kernel.h"
#include "../os/kernel/scheduler.h"
#include "../os/mem/memory.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

#ifdef LOCAL_MODE

static uint8_t local_memory[512 * 1024] __attribute__((aligned(4)));
static uint8_t local_stack_mem[S3_MAX_TASKS * S3_TASK_STACK]
    __attribute__((aligned(4)));

uint8_t* s3_task_stack_base = local_stack_mem;
volatile uint32_t s3_tick_counter = 0;
volatile uint32_t s3_vsync_counter = 0;

void s3_set_interrupts(void) {}
void s3_restore_interrupts(void) {}
void s3_disable_interrupts(void) {}
void s3_enable_interrupts(void) {}

#define GVRAM ((volatile uint16_t*)0xC00000)
#define GVRAM_STR 512
#define SW 512
#define SH 512
#define DISP_W 512
#define DISP_H 512

/* Palette Indices (1 byte per pixel in Mode 9) */
#define C_WHITE   0
#define C_BLACK   215
#define C_GRAY_L  247
#define C_GRAY_M  250
#define C_GRAY_D  252

/* X68k Palette Helper (GGGGG RRRRR BBBBB P) */
/* Force Bit 15 (Green MSB) and Bit 0 (Priority) to 1 for maximum brightness */
#define PAL_RGB(r, g, b)                                                       \
    (uint16_t)((((g) & 0x1F) << 11) | (((r) & 0x1F) << 6) | (((b) & 0x1F) << 1) | 1)

static void set_palette(void) {
    int i, r, g, b;
    int idx = 0;
    static const uint8_t cube_levels[] = {31, 25, 18, 12, 6, 0};
    static const uint8_t system_levels[] = {29, 27, 23, 21, 17, 15, 10, 8, 4, 2};

    /* 1. 6x6x6 Color Cube (Indices 0-215) */
    for (r = 0; r < 6; r++) {
        for (g = 0; g < 6; g++) {
            for (b = 0; b < 6; b++) {
                _iocs_gpalet(idx++, PAL_RGB(cube_levels[r], cube_levels[g], cube_levels[b]));
            }
        }
    }

    /* 2. 40 System Colors (Indices 216-255) */
    /* Reds (216-225) */
    for (i = 0; i < 10; i++) _iocs_gpalet(idx++, PAL_RGB(system_levels[i], 0, 0));
    /* Greens (226-235) */
    for (i = 0; i < 10; i++) _iocs_gpalet(idx++, PAL_RGB(0, system_levels[i], 0));
    /* Blues (236-245) */
    for (i = 0; i < 10; i++) _iocs_gpalet(idx++, PAL_RGB(0, 0, system_levels[i]));
    /* Grays (246-255) */
    for (i = 0; i < 10; i++) _iocs_gpalet(idx++, PAL_RGB(system_levels[i], system_levels[i], system_levels[i]));
}

#define CGROM ((const uint8_t*)0xF00000)

/* Layout for 8x8 font */
#define TITLE_H 12
#define CONTENT_Y 14
#define LINE_H 10
#define WIN_W 240
#define WIN_H (CONTENT_Y + 3 * LINE_H + 4) /* 48 */

typedef struct {
    int x, y, w, h;
    char title[20];
    char line[3][30];
    char prev[3][30];
} Win;

static Win wins[3];
static int zmap[3] = {0, 1, 2};
static volatile uint32_t frame = 0;
static int last_key = -1;
static int mx = DISP_W / 2, my = DISP_H / 2;
static int drag = -1, dox, doy;
static int need_full = 1;

#define OL_MAX 1200
static uint16_t ol_buf[OL_MAX];
static int ol_x, ol_y, ol_w, ol_h, ol_valid;

/* ================================================================
 * Drawing
 * ================================================================ */

static void fill_rect(int x0, int y0, int x1, int y1, uint8_t c) {
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 >= DISP_W)
        x1 = DISP_W - 1;
    if (y1 >= DISP_H)
        y1 = DISP_H - 1;
    if (x0 > x1 || y0 > y1)
        return;
    for (int y = y0; y <= y1; y++) {
        volatile uint16_t* b = GVRAM + y * GVRAM_STR;
        for (int x = x0; x <= x1; x++) b[x] = c;
    }
}

/* MacOS 7 style desktop pattern (Checkerboard) */
static void fill_stipple(int x0, int y0, int x1, int y1, uint8_t c1,
                         uint8_t c2) {
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 >= DISP_W)
        x1 = DISP_W - 1;
    if (y1 >= DISP_H)
        y1 = DISP_H - 1;
    for (int y = y0; y <= y1; y++) {
        volatile uint16_t* b = GVRAM + y * GVRAM_STR;
        for (int x = x0; x <= x1; x++) {
            b[x] = ((x + y) & 1) ? c1 : c2;
        }
    }
}

/* Spleen 5x8 Font Data (ASCII 0x20 - 0x7E) */
static const uint8_t spleen_5x8[][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 0x20 Space */
    {0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x20, 0x00}, /* 0x21 ! */
    {0x50, 0x50, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 0x22 " */
    {0x00, 0x50, 0xF8, 0x50, 0x50, 0xF8, 0x50, 0x00}, /* 0x23 # */
    {0x20, 0x70, 0xA0, 0x60, 0x30, 0x30, 0xE0, 0x20}, /* 0x24 $ */
    {0x10, 0x90, 0xA0, 0x20, 0x40, 0x50, 0x90, 0x80}, /* 0x25 % */
    {0x20, 0x50, 0x50, 0x60, 0xA8, 0x90, 0x68, 0x00}, /* 0x26 & */
    {0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 0x27 ' */
    {0x10, 0x20, 0x40, 0x40, 0x40, 0x40, 0x20, 0x10}, /* 0x28 ( */
    {0x40, 0x20, 0x10, 0x10, 0x10, 0x10, 0x20, 0x40}, /* 0x29 ) */
    {0x00, 0x00, 0x90, 0x60, 0xF0, 0x60, 0x90, 0x00}, /* 0x2A * */
    {0x00, 0x00, 0x20, 0x20, 0xF8, 0x20, 0x20, 0x00}, /* 0x2B + */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x40}, /* 0x2C , */
    {0x00, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x00, 0x00}, /* 0x2D - */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00}, /* 0x2E . */
    {0x10, 0x10, 0x20, 0x20, 0x40, 0x40, 0x80, 0x80}, /* 0x2F / */
    {0x00, 0x60, 0x90, 0xB0, 0xD0, 0x90, 0x60, 0x00}, /* 0x30 0 */
    {0x00, 0x20, 0x60, 0x20, 0x20, 0x20, 0x70, 0x00}, /* 0x31 1 */
    {0x00, 0x60, 0x90, 0x10, 0x60, 0x80, 0xF0, 0x00}, /* 0x32 2 */
    {0x00, 0x60, 0x90, 0x20, 0x10, 0x90, 0x60, 0x00}, /* 0x33 3 */
    {0x00, 0x80, 0xA0, 0xA0, 0xF0, 0x20, 0x20, 0x00}, /* 0x34 4 */
    {0x00, 0xF0, 0x80, 0xE0, 0x10, 0x10, 0xE0, 0x00}, /* 0x35 5 */
    {0x00, 0x60, 0x80, 0xE0, 0x90, 0x90, 0x60, 0x00}, /* 0x36 6 */
    {0x00, 0xF0, 0x90, 0x10, 0x20, 0x40, 0x40, 0x00}, /* 0x37 7 */
    {0x00, 0x60, 0x90, 0x60, 0x90, 0x90, 0x60, 0x00}, /* 0x38 8 */
    {0x00, 0x60, 0x90, 0x90, 0x70, 0x10, 0x60, 0x00}, /* 0x39 9 */
    {0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x20, 0x00}, /* 0x3A : */
    {0x00, 0x00, 0x00, 0x20, 0x00, 0x20, 0x20, 0x40}, /* 0x3B ; */
    {0x00, 0x10, 0x20, 0x40, 0x40, 0x20, 0x10, 0x00}, /* 0x3C < */
    {0x00, 0x00, 0x00, 0xF0, 0x00, 0xF0, 0x00, 0x00}, /* 0x3D = */
    {0x00, 0x40, 0x20, 0x10, 0x10, 0x20, 0x40, 0x00}, /* 0x3E > */
    {0x60, 0x90, 0x10, 0x20, 0x40, 0x00, 0x40, 0x00}, /* 0x3F ? */
    {0x00, 0x60, 0x90, 0xB0, 0xB0, 0x80, 0x70, 0x00}, /* 0x40 @ */
    {0x00, 0x60, 0x90, 0x90, 0xF0, 0x90, 0x90, 0x00}, /* 0x41 A */
    {0x00, 0xE0, 0x90, 0xE0, 0x90, 0x90, 0xE0, 0x00}, /* 0x42 B */
    {0x00, 0x70, 0x80, 0x80, 0x80, 0x80, 0x70, 0x00}, /* 0x43 C */
    {0x00, 0xE0, 0x90, 0x90, 0x90, 0x90, 0xE0, 0x00}, /* 0x44 D */
    {0x00, 0x70, 0x80, 0xE0, 0x80, 0x80, 0x70, 0x00}, /* 0x45 E */
    {0x00, 0x70, 0x80, 0x80, 0xE0, 0x80, 0x80, 0x00}, /* 0x46 F */
    {0x00, 0x70, 0x80, 0xB0, 0x90, 0x90, 0x70, 0x00}, /* 0x47 G */
    {0x00, 0x90, 0x90, 0xF0, 0x90, 0x90, 0x90, 0x00}, /* 0x48 H */
    {0x00, 0x70, 0x20, 0x20, 0x20, 0x20, 0x70, 0x00}, /* 0x49 I */
    {0x00, 0x70, 0x20, 0x20, 0x20, 0x20, 0xC0, 0x00}, /* 0x4A J */
    {0x00, 0x90, 0x90, 0xE0, 0x90, 0x90, 0x90, 0x00}, /* 0x4B K */
    {0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x70, 0x00}, /* 0x4C L */
    {0x00, 0x90, 0xF0, 0xF0, 0x90, 0x90, 0x90, 0x00}, /* 0x4D M */
    {0x00, 0x90, 0xD0, 0xD0, 0xB0, 0xB0, 0x90, 0x00}, /* 0x4E N */
    {0x00, 0x60, 0x90, 0x90, 0x90, 0x90, 0x60, 0x00}, /* 0x4F O */
    {0x00, 0xE0, 0x90, 0x90, 0xE0, 0x80, 0x80, 0x00}, /* 0x50 P */
    {0x00, 0x60, 0x90, 0x90, 0x90, 0x90, 0x60, 0x30}, /* 0x51 Q */
    {0x00, 0xE0, 0x90, 0x90, 0xE0, 0x90, 0x90, 0x00}, /* 0x52 R */
    {0x00, 0x70, 0x80, 0x60, 0x10, 0x10, 0xE0, 0x00}, /* 0x53 S */
    {0x00, 0xF8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00}, /* 0x54 T */
    {0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x70, 0x00}, /* 0x55 U */
    {0x00, 0x90, 0x90, 0x90, 0x90, 0x60, 0x60, 0x00}, /* 0x56 V */
    {0x00, 0x90, 0x90, 0x90, 0xF0, 0xF0, 0x90, 0x00}, /* 0x57 W */
    {0x00, 0x90, 0x90, 0x60, 0x60, 0x90, 0x90, 0x00}, /* 0x58 X */
    {0x00, 0x90, 0x90, 0x90, 0x70, 0x10, 0xE0, 0x00}, /* 0x59 Y */
    {0x00, 0xF0, 0x10, 0x20, 0x40, 0x80, 0xF0, 0x00}, /* 0x5A Z */
    {0x70, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x70}, /* 0x5B [ */
    {0x80, 0x80, 0x40, 0x40, 0x20, 0x20, 0x10, 0x10}, /* 0x5C \ */
    {0x70, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x70}, /* 0x5D ] */
    {0x00, 0x20, 0x50, 0x88, 0x00, 0x00, 0x00, 0x00}, /* 0x5E ^ */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0}, /* 0x5F _ */
    {0x40, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* 0x60 ` */
    {0x00, 0x00, 0x60, 0x10, 0x70, 0x90, 0x70, 0x00}, /* 0x61 a */
    {0x80, 0x80, 0xE0, 0x90, 0x90, 0x90, 0xE0, 0x00}, /* 0x62 b */
    {0x00, 0x00, 0x70, 0x80, 0x80, 0x80, 0x70, 0x00}, /* 0x63 c */
    {0x10, 0x10, 0x70, 0x90, 0x90, 0x90, 0x70, 0x00}, /* 0x64 d */
    {0x00, 0x00, 0x70, 0x90, 0xF0, 0x80, 0x70, 0x00}, /* 0x65 e */
    {0x30, 0x40, 0x40, 0xE0, 0x40, 0x40, 0x40, 0x00}, /* 0x66 f */
    {0x00, 0x00, 0x70, 0x90, 0x90, 0x60, 0x10, 0xE0}, /* 0x67 g */
    {0x80, 0x80, 0xE0, 0x90, 0x90, 0x90, 0x90, 0x00}, /* 0x68 h */
    {0x00, 0x20, 0x00, 0x60, 0x20, 0x20, 0x30, 0x00}, /* 0x69 i */
    {0x00, 0x20, 0x00, 0x20, 0x20, 0x20, 0x20, 0xC0}, /* 0x6A j */
    {0x80, 0x80, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x00}, /* 0x6B k */
    {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x30, 0x00}, /* 0x6C l */
    {0x00, 0x00, 0x90, 0xF0, 0xF0, 0x90, 0x90, 0x00}, /* 0x6D m */
    {0x00, 0x00, 0xE0, 0x90, 0x90, 0x90, 0x90, 0x00}, /* 0x6E n */
    {0x00, 0x00, 0x60, 0x90, 0x90, 0x90, 0x60, 0x00}, /* 0x6F o */
    {0x00, 0x00, 0xE0, 0x90, 0x90, 0xE0, 0x80, 0x80}, /* 0x70 p */
    {0x00, 0x00, 0x70, 0x90, 0x90, 0x70, 0x10, 0x10}, /* 0x71 q */
    {0x00, 0x00, 0x70, 0x90, 0x80, 0x80, 0x80, 0x00}, /* 0x72 r */
    {0x00, 0x00, 0x70, 0x80, 0x60, 0x10, 0xE0, 0x00}, /* 0x73 s */
    {0x40, 0x40, 0xE0, 0x40, 0x40, 0x40, 0x30, 0x00}, /* 0x74 t */
    {0x00, 0x00, 0x90, 0x90, 0x90, 0x90, 0x70, 0x00}, /* 0x75 u */
    {0x00, 0x00, 0x90, 0x90, 0x90, 0x60, 0x60, 0x00}, /* 0x76 v */
    {0x00, 0x00, 0x90, 0x90, 0xF0, 0xF0, 0x90, 0x00}, /* 0x77 w */
    {0x00, 0x00, 0x90, 0x60, 0x60, 0x90, 0x90, 0x00}, /* 0x78 x */
    {0x00, 0x00, 0x90, 0x90, 0x90, 0x70, 0x10, 0xE0}, /* 0x79 y */
    {0x00, 0x00, 0xF0, 0x10, 0x20, 0x40, 0xF0, 0x00}, /* 0x7A z */
    {0x30, 0x40, 0x40, 0xC0, 0xC0, 0x40, 0x40, 0x30}, /* 0x7B { */
    {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20}, /* 0x7C | */
    {0xC0, 0x20, 0x20, 0x30, 0x30, 0x20, 0x20, 0xC0}, /* 0x7D } */
    {0x00, 0x00, 0x00, 0x48, 0xB0, 0x00, 0x00, 0x00}, /* 0x7E ~ */
};

static void draw_char8(int px, int py, char ch, uint8_t fg, uint8_t bg) {
    uint8_t c = (uint8_t)ch;
    if (c < 0x20 || c > 0x7E)
        c = ' ';
    const uint8_t* g = spleen_5x8[c - 0x20];
    for (int r = 0; r < 8; r++) {
        int yy = py + r;
        if (yy < 0 || yy >= DISP_H)
            continue;
        volatile uint16_t* row = GVRAM + yy * GVRAM_STR;
        uint8_t bits = g[r];
        for (int b = 0; b < 5; b++) { /* Spleen is 5px wide */
            int xx = px + b;
            if (xx >= 0 && xx < DISP_W)
                row[xx] = (bits & (0x80 >> b)) ? fg : bg;
        }
    }
}

static void draw_text(int px, int py, const char* s, uint8_t fg, uint8_t bg) {
    while (*s) {
        draw_char8(px, py, *s++, fg, bg);
        px += 6; /* 5px + 1px gap */
    }
}

/* Draw char, skipping pixels covered by windows above zmap[zpos] */
static void draw_char8_clip(int px, int py, char ch, uint8_t fg, uint8_t bg,
                            int zpos) {
    uint8_t c = (uint8_t)ch;
    if (c < 0x20 || c > 0x7E)
        c = ' ';
    const uint8_t* g = spleen_5x8[c - 0x20];
    for (int r = 0; r < 8; r++) {
        int yy = py + r;
        if (yy < 0 || yy >= DISP_H)
            continue;
        volatile uint16_t* row = GVRAM + yy * GVRAM_STR;
        uint8_t bits = g[r];
        for (int b = 0; b < 5; b++) {
            int xx = px + b;
            if (xx < 0 || xx >= DISP_W)
                continue;
            int covered = 0;
            for (int k = zpos + 1; k < 3; k++) {
                Win* ow = &wins[zmap[k]];
                if (xx >= ow->x && xx < ow->x + ow->w &&
                    yy >= ow->y && yy < ow->y + ow->h) {
                    covered = 1;
                    break;
                }
            }
            if (!covered)
                row[xx] = (bits & (0x80 >> b)) ? fg : bg;
        }
    }
}

static void draw_text_clip(int px, int py, const char* s, uint8_t fg,
                           uint8_t bg, int zpos) {
    while (*s) {
        draw_char8_clip(px, py, *s++, fg, bg, zpos);
        px += 6;
    }
}

static void pad(char* s, int n) {
    int l = (int)strlen(s);
    for (int i = l; i < n; i++) s[i] = ' ';
    s[n] = '\0';
}

static void draw_content_dirty(Win* w) {
    for (int i = 0; i < 3; i++) {
        if (memcmp(w->line[i], w->prev[i], 30) != 0) {
            draw_text(w->x + 4, w->y + CONTENT_Y + i * LINE_H, w->line[i],
                      C_BLACK, C_WHITE);
            memcpy(w->prev[i], w->line[i], 30);
        }
    }
}

static void draw_frame(Win* w, int is_fg) {
    /* Window frame: Black */
    fill_rect(w->x, w->y, w->x + w->w - 1, w->y + w->h - 1, C_BLACK);
    /* Content area: White (classic Mac) */
    fill_rect(w->x + 1, w->y + TITLE_H, w->x + w->w - 2, w->y + w->h - 2,
              C_WHITE);

    /* Title bar background */
    uint16_t t_bg = is_fg ? C_GRAY_L : C_WHITE;
    fill_rect(w->x + 1, w->y + 1, w->x + w->w - 2, w->y + TITLE_H - 2, t_bg);

    /* Bottom line of title bar (MacOS style separation) */
    fill_rect(w->x + 1, w->y + TITLE_H - 1, w->x + w->w - 2, w->y + TITLE_H - 1,
              C_BLACK);

    int tw = (int)strlen(w->title) * 6;
    int tx = w->x + (w->w - tw) / 2;

    /* Foreground: 5 horizontal lines */
    if (is_fg) {
        for (int i = 0; i < 5; i++) {
            int ly = w->y + 2 + i * 2;
            /* Left of title (with clean margin) */
            if (tx > w->x + 12)
                fill_rect(w->x + 4, ly, tx - 8, ly, C_BLACK);
            /* Right of title (with clean margin) */
            if (tx + tw + 8 < w->x + w->w - 4)
                fill_rect(tx + tw + 8, ly, w->x + w->w - 5, ly, C_BLACK);
        }
    }

    /* Title text */
    draw_text(tx, w->y + 2, w->title, C_BLACK, t_bg);
}

/* MacOS 7 style marching ants outline */
static void draw_march_outline(int x, int y, int w, int h) {
    int offset = (int)(frame & 7); /* Animate every frame */
    uint16_t colors[2] = {C_WHITE, C_BLACK};
    /* Walk perimeter: top → right → bottom → left */
    int peri = 0;
    /* Top edge (left to right) */
    for (int i = 0; i < w; i++, peri++) {
        int px = x + i;
        if (px >= 0 && px < DISP_W && y >= 0 && y < DISP_H)
            GVRAM[y * GVRAM_STR + px] = colors[((peri + offset) >> 2) & 1];
    }
    /* Right edge (top to bottom) */
    for (int i = 1; i < h; i++, peri++) {
        int py = y + i;
        if (x + w - 1 >= 0 && x + w - 1 < DISP_W && py >= 0 && py < DISP_H)
            GVRAM[py * GVRAM_STR + x + w - 1] =
                colors[((peri + offset) >> 2) & 1];
    }
    /* Bottom edge (right to left) */
    for (int i = w - 2; i >= 0; i--, peri++) {
        int px = x + i;
        if (px >= 0 && px < DISP_W && y + h - 1 >= 0 && y + h - 1 < DISP_H)
            GVRAM[(y + h - 1) * GVRAM_STR + px] =
                colors[((peri + offset) >> 2) & 1];
    }
    /* Left edge (bottom to top) */
    for (int i = h - 2; i >= 1; i--, peri++) {
        int py = y + i;
        if (x >= 0 && x < DISP_W && py >= 0 && py < DISP_H)
            GVRAM[py * GVRAM_STR + x] = colors[((peri + offset) >> 2) & 1];
    }
}

/* ---- Outline save / restore ---- */

static void ol_save(int x, int y, int w, int h) {
    ol_x = x;
    ol_y = y;
    ol_w = w;
    ol_h = h;
    int idx = 0;
    for (int i = 0; i < w && idx < OL_MAX; i++) {
        int px = x + i;
        ol_buf[idx++] = (px >= 0 && px < SW && y >= 0 && y < SH)
                            ? GVRAM[y * GVRAM_STR + px]
                            : 0;
    }
    for (int i = 0; i < w && idx < OL_MAX; i++) {
        int px = x + i, py = y + h - 1;
        ol_buf[idx++] = (px >= 0 && px < SW && py >= 0 && py < SH)
                            ? GVRAM[py * GVRAM_STR + px]
                            : 0;
    }
    for (int i = 1; i < h - 1 && idx < OL_MAX; i++) {
        int py = y + i;
        ol_buf[idx++] = (x >= 0 && x < SW && py >= 0 && py < SH)
                            ? GVRAM[py * GVRAM_STR + x]
                            : 0;
    }
    for (int i = 1; i < h - 1 && idx < OL_MAX; i++) {
        int py = y + i, px = x + w - 1;
        ol_buf[idx++] = (px >= 0 && px < SW && py >= 0 && py < SH)
                            ? GVRAM[py * GVRAM_STR + px]
                            : 0;
    }
    ol_valid = 1;
}

static void ol_restore(void) {
    if (!ol_valid)
        return;
    int x = ol_x, y = ol_y, w = ol_w, h = ol_h;
    int idx = 0;
    for (int i = 0; i < w && idx < OL_MAX; i++) {
        int px = x + i;
        if (px >= 0 && px < SW && y >= 0 && y < SH)
            GVRAM[y * GVRAM_STR + px] = ol_buf[idx];
        idx++;
    }
    for (int i = 0; i < w && idx < OL_MAX; i++) {
        int px = x + i, py = y + h - 1;
        if (px >= 0 && px < SW && py >= 0 && py < SH)
            GVRAM[py * GVRAM_STR + px] = ol_buf[idx];
        idx++;
    }
    for (int i = 1; i < h - 1 && idx < OL_MAX; i++) {
        int py = y + i;
        if (x >= 0 && x < SW && py >= 0 && py < SH)
            GVRAM[py * GVRAM_STR + x] = ol_buf[idx];
        idx++;
    }
    for (int i = 1; i < h - 1 && idx < OL_MAX; i++) {
        int py = y + i, px = x + w - 1;
        if (px >= 0 && px < SW && py >= 0 && py < SH)
            GVRAM[py * GVRAM_STR + px] = ol_buf[idx];
        idx++;
    }
    ol_valid = 0;
}

/* ================================================================
 * Z-order
 * ================================================================ */

static void bring_to_front(int idx) {
    int p = -1;
    for (int i = 0; i < 3; i++)
        if (zmap[i] == idx) {
            p = i;
            break;
        }
    if (p < 0 || p == 2)
        return;
    for (int i = p; i < 2; i++) zmap[i] = zmap[i + 1];
    zmap[2] = idx;
}

static int hit_test(int hx, int hy) {
    for (int i = 2; i >= 0; i--) {
        Win* w = &wins[zmap[i]];
        if (hx >= w->x && hx < w->x + w->w && hy >= w->y && hy < w->y + w->h)
            return zmap[i];
    }
    return -1;
}

/* ================================================================
 * V-sync
 * ================================================================ */

static void wait_vsync(void) {
    volatile uint8_t* gpip = (volatile uint8_t*)0xE88001;
    while (!(*gpip & 0x10));
    while (*gpip & 0x10);
    frame++;
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    int ssp = _iocs_b_super(0);

    s3_mem_init(local_memory, sizeof(local_memory));
    s3_sched_init();

    int old_mode = _iocs_crtmod(-1);
    _iocs_crtmod(9); /* 512x512 256-color */
    _iocs_g_clr_on();
    set_palette();
    _iocs_b_curoff();
    _iocs_skeyset(0);

    {
        volatile uint32_t* tp = (volatile uint32_t*)0xE00000;
        for (int i = 0; i < 0x1000; i++) tp[i] = 0;
    }

    _iocs_ms_init();
    _iocs_skey_mod(0, 0, 0);
    _iocs_ms_curon();
    _iocs_ms_limit(0, DISP_W - 1, 0, DISP_H - 1);

    wins[0] = (Win){30, 15, WIN_W, WIN_H, "Timer", {{{0}}}, {{0}}};
    wins[1] = (Win){180, 60, WIN_W, WIN_H, "Keyboard", {{{0}}}, {{0}}};
    wins[2] = (Win){80, 120, WIN_W, WIN_H, "Mouse", {{{0}}}, {{0}}};
    ol_valid = 0;

    for (;;) {
        wait_vsync();

        if (_iocs_b_keysns() > 0) {
            int key = _iocs_b_keyinp();
            if ((key & 0xFF) == 0x1B)
                break;
            last_key = key;
        }

        int dt = _iocs_ms_getdt();
        {
            int pos = _iocs_ms_curgt();
            mx = (int16_t)((pos >> 16) & 0xFFFF);
            my = (int16_t)(pos & 0xFFFF);
        }
        int left = ((dt >> 8) & 0xFF) != 0;

        if (left && drag < 0) {
            int hit = hit_test(mx, my);
            if (hit >= 0 && my >= wins[hit].y + 1 &&
                my <= wins[hit].y + TITLE_H) {
                drag = hit;
                dox = mx - wins[hit].x;
                doy = my - wins[hit].y;
                bring_to_front(hit);
                fill_stipple(wins[hit].x, wins[hit].y,
                             wins[hit].x + wins[hit].w - 1,
                             wins[hit].y + wins[hit].h - 1, C_WHITE,
                             C_GRAY_M);
                for (int i = 0; i < 3; i++) {
                    int idx = zmap[i];
                    if (idx == drag)
                        continue;
                    draw_frame(&wins[idx], (i == 2));
                    memset(wins[idx].prev, 0xFF, sizeof(wins[idx].prev));
                    draw_content_dirty(&wins[idx]);
                }
                ol_save(wins[drag].x, wins[drag].y, wins[drag].w,
                        wins[drag].h);
                draw_march_outline(wins[drag].x, wins[drag].y,
                                   wins[drag].w, wins[drag].h);
                memset(wins[drag].prev, 0xFF, sizeof(wins[drag].prev));
                need_full = 0;
            }
        }
        if (!left && drag >= 0) {
            ol_restore();
            draw_frame(&wins[drag], 1);
            memset(wins[drag].prev, 0xFF, sizeof(wins[drag].prev));
            draw_content_dirty(&wins[drag]);
            drag = -1;
        }
        if (drag >= 0) {
            wins[drag].x = mx - dox;
            wins[drag].y = my - doy;
        }

        {
            uint32_t sec = frame / 57;
            uint32_t frac = (frame % 57) * 100 / 57;
            sprintf(wins[0].line[0], "Vsync: %lu", frame);
            pad(wins[0].line[0], 24);
            sprintf(wins[0].line[1], "Time: %lu.%02lu", sec, frac);
            pad(wins[0].line[1], 24);
            sprintf(wins[0].line[2], "Frame: %lu/57", frame % 57);
            pad(wins[0].line[2], 24);
        }
        {
            if (last_key >= 0) {
                int k = last_key & 0xFF;
                char ch = (k >= 0x20 && k < 0x7F) ? (char)k : '.';
                sprintf(wins[1].line[0], "Code:%02XH '%c'", k, ch);
                pad(wins[1].line[0], 24);
                sprintf(wins[1].line[1], "Shift:%02XH", (last_key >> 8) & 0xFF);
                pad(wins[1].line[1], 24);
            } else {
                strcpy(wins[1].line[0], "Press any key...  ");
                pad(wins[1].line[0], 24);
                strcpy(wins[1].line[1], "                        ");
            }
            strcpy(wins[1].line[2], "                        ");
        }
        {
            sprintf(wins[2].line[0], "X:%3d Y:%3d", mx, my);
            pad(wins[2].line[0], 24);
            sprintf(wins[2].line[1], "L=%s", left ? "DN" : "UP");
            pad(wins[2].line[1], 24);
            strcpy(wins[2].line[2], "                        ");
        }

        if (need_full || drag >= 0 || (frame % 11 == 0)) {
        if (need_full) {
            fill_stipple(0, 0, DISP_W - 1, DISP_H - 1, C_WHITE, C_GRAY_M);
            for (int i = 0; i < 3; i++) {
                int idx = zmap[i];
                draw_frame(&wins[idx], (i == 2));
                memset(wins[idx].prev, 0xFF, sizeof(wins[idx].prev));
                draw_content_dirty(&wins[idx]);
            }
            need_full = 0;
        } else if (drag >= 0) {
            ol_restore();
            ol_save(wins[drag].x, wins[drag].y, wins[drag].w, wins[drag].h);
            draw_march_outline(wins[drag].x, wins[drag].y, wins[drag].w,
                               wins[drag].h);
        } else {
            for (int i = 0; i < 3; i++) {
                int idx = zmap[i];
                Win* w = &wins[idx];
                for (int j = 0; j < 3; j++) {
                    if (memcmp(w->line[j], w->prev[j], 30) != 0) {
                        draw_text_clip(w->x + 4,
                                       w->y + CONTENT_Y + j * LINE_H,
                                       w->line[j], C_BLACK, C_WHITE, i);
                        memcpy(w->prev[j], w->line[j], 30);
                    }
                }
            }
        }
        } /* render gate */
    }

    ol_restore();
    _iocs_ms_curof();
    _iocs_skey_mod(-1, 0, 0);
    _iocs_crtmod(old_mode);
    _iocs_b_curon();
    _iocs_b_print("SSOS3 terminated.\r\n");
    _iocs_b_super(ssp);
    return 0;
}

#endif /* LOCAL_MODE */
