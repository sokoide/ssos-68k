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
#define GVRAM_STR   512
#define SW          512
#define SH          512
#define DISP_W      512
#define DISP_H      512

#define RGB(r, g, b)                                                           \
    ((((g) & 0x1F) << 11) | (((r) & 0x1F) << 6) | (((b) & 0x1F) << 1))

#define C_BG RGB(0, 0, 3)
#define C_WHITE RGB(31, 31, 31)
#define C_BLACK 0x0000
#define C_BORDER0 RGB(25, 25, 30)
#define C_TITLE0 RGB(0, 12, 24)
#define C_BORDER1 RGB(22, 22, 30)
#define C_TITLE1 RGB(0, 16, 28)
#define C_BORDER2 RGB(20, 26, 24)
#define C_TITLE2 RGB(0, 20, 16)
#define CGROM ((const uint8_t*)0xF00000)

/* Layout for 8x8 font */
#define TITLE_H 10
#define CONTENT_Y 12
#define LINE_H 10
#define WIN_W 240
#define WIN_H (CONTENT_Y + 3 * LINE_H + 4) /* 46 */

typedef struct {
    int x, y, w, h;
    uint16_t border, title_bg;
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

static void fill_rect(int x0, int y0, int x1, int y1, uint16_t c) {
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
    uint32_t c32 = ((uint32_t)c << 16) | c;
    for (int y = y0; y <= y1; y++) {
        volatile uint16_t* b = GVRAM + y * GVRAM_STR;
        int x = x0;
        if (x & 1)
            b[x++] = c;
        for (; x + 1 <= x1; x += 2) *(volatile uint32_t*)(b + x) = c32;
        if (x <= x1)
            b[x] = c;
    }
}

static void draw_char8(int px, int py, char ch, uint16_t fg, uint16_t bg) {
    uint8_t c = (uint8_t)ch;
    if (c < 0x20 || (c > 0x7E && c < 0xA1) || c > 0xDF)
        c = ' ';
    /* Read 8x8 ANK font at CGROM+$F3A000, 8 bytes per character (packed layout)
     */
    const uint8_t* g = CGROM + 0x3A000 + c * 8;
    for (int r = 0; r < 8; r++) {
        int yy = py + r;
        if (yy < 0 || yy >= DISP_H)
            continue;
        volatile uint16_t* row = GVRAM + yy * GVRAM_STR;
        uint8_t bits = g[r];
        for (int b = 0; b < 8; b++) {
            int xx = px + b;
            if (xx >= 0 && xx < DISP_W)
                row[xx] = (bits & (0x80 >> b)) ? fg : bg;
        }
    }
}

static void draw_text(int px, int py, const char* s, uint16_t fg, uint16_t bg) {
    while (*s) {
        draw_char8(px, py, *s++, fg, bg);
        px += 8;
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
                      C_WHITE, C_BLACK);
            memcpy(w->prev[i], w->line[i], 30);
        }
    }
}

static void draw_frame(Win* w) {
    fill_rect(w->x, w->y, w->x + w->w - 1, w->y + w->h - 1, w->border);
    fill_rect(w->x + 1, w->y + TITLE_H, w->x + w->w - 2, w->y + w->h - 2,
              C_BLACK);
    fill_rect(w->x + 1, w->y + 1, w->x + w->w - 2, w->y + TITLE_H - 1,
              w->title_bg);
    draw_text(w->x + 4, w->y + 1, w->title, C_BLACK, w->title_bg);
}

static void draw_outline(int x, int y, int w, int h) {
    fill_rect(x, y, x + w - 1, y, C_WHITE);
    fill_rect(x, y + h - 1, x + w - 1, y + h - 1, C_WHITE);
    fill_rect(x, y, x, y + h - 1, C_WHITE);
    fill_rect(x + w - 1, y, x + w - 1, y + h - 1, C_WHITE);
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

    _iocs_crtmod(8); /* 512x512 65536-color */
    _iocs_g_clr_on();
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

    wins[0] = (Win){30,       15,      WIN_W,   WIN_H, C_BORDER0,
                    C_TITLE0, "Timer", {{{0}}}, {{0}}};
    wins[1] = (Win){180,      60,         WIN_W,   WIN_H, C_BORDER1,
                    C_TITLE1, "Keyboard", {{{0}}}, {{0}}};
    wins[2] = (Win){80,       120,     WIN_W,   WIN_H, C_BORDER2,
                    C_TITLE2, "Mouse", {{{0}}}, {{0}}};
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
                need_full = 1;
            }
        }
        if (!left && drag >= 0) {
            ol_restore();
            drag = -1;
            need_full = 1;
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

        if (need_full) {
            fill_rect(0, 0, DISP_W - 1, DISP_H - 1, C_BG);
            for (int i = 0; i < 3; i++) {
                int idx = zmap[i];
                if (drag >= 0 && idx == drag)
                    continue;
                draw_frame(&wins[idx]);
                memset(wins[idx].prev, 0xFF, sizeof(wins[idx].prev));
                draw_content_dirty(&wins[idx]);
            }
            if (drag >= 0) {
                ol_save(wins[drag].x, wins[drag].y, wins[drag].w, wins[drag].h);
                draw_outline(wins[drag].x, wins[drag].y, wins[drag].w,
                             wins[drag].h);
            }
            need_full = 0;
        } else if (drag >= 0) {
            ol_restore();
            ol_save(wins[drag].x, wins[drag].y, wins[drag].w, wins[drag].h);
            draw_outline(wins[drag].x, wins[drag].y, wins[drag].w,
                         wins[drag].h);
        } else {
            for (int i = 0; i < 3; i++) draw_content_dirty(&wins[zmap[i]]);
        }
    }

    ol_restore();
    _iocs_ms_curof();
    _iocs_skey_mod(-1, 0, 0);
    _iocs_crtmod(0);
    _iocs_b_curon();
    _iocs_b_print("SSOS3 terminated.\r\n");
    _iocs_b_super(ssp);
    return 0;
}

#endif /* LOCAL_MODE */
