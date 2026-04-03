/*
 * SSOS2 GUI Engine — Multi-Window with Back Buffer
 *
 * Architecture: Back buffer → Dirty regions → VRAM
 *
 * All drawing targets backbuf[] in main RAM (384KB).
 * No CRTC bus contention during drawing = fast & predictable.
 * Only changed regions are transferred to VRAM during V-blank.
 *
 * Multi-window support:
 *   g_wins[] holds window definitions in z-order (bottom=0, top=n-1).
 *   Background tasks set .dirty=1 when their content changes.
 *   The main task redraws dirty windows and transfers to VRAM.
 */

#include "gui.h"
#include <string.h>
#include <x68k/iocs.h>

/* --- Back buffer: 1 byte/pixel, SCREEN_W × SCREEN_H (384KB) --- */
uint8_t backbuf[SCREEN_H][SCREEN_W];

/* --- Global tick counter (written by Timer D ISR) --- */
volatile uint32_t ssos2_tick = 0;

/* --- Window table (z-order: [0]=bottom, [n-1]=top) --- */
AppWin g_wins[MAX_WINDOWS];
int g_num_wins = 0;

/* --- Z-order map: g_zorder[0]=bottom index, g_zorder[top]=top index --- */
static int g_zorder[MAX_WINDOWS];

/* ===========================================================
 * Initialization
 * =========================================================== */

void gui_init(void) {
    _iocs_crtmod(16); /* 768×512, 16 colors */
    _iocs_g_clr_on(); /* Clear VRAM, reset palette, page 0 */
    _iocs_b_curoff(); /* Hide text cursor */

    gui_set_palette();
    memset(backbuf, 0, sizeof(backbuf));
    memset(g_wins, 0, sizeof(g_wins));
    g_num_wins = 0;
}

void gui_set_palette(void) {
    /* Set GRAPHIC screen palette via IOCS TPALET2.
     * Color code format: 0xRGB (4-bit per component, 12-bit total) */
    _iocs_tpalet2(COL_BLACK, 0x000);    /* 0  */
    _iocs_tpalet2(COL_BLUE, 0x00A);     /* 1  */
    _iocs_tpalet2(COL_RED, 0xF22);      /* 2  */
    _iocs_tpalet2(COL_MAGENTA, 0xF0A);  /* 3  */
    _iocs_tpalet2(COL_GREEN, 0x0C0);    /* 4  */
    _iocs_tpalet2(COL_CYAN, 0x0CC);     /* 5  */
    _iocs_tpalet2(COL_YELLOW, 0xEE0);   /* 6  */
    _iocs_tpalet2(COL_WHITE, 0xFFF);    /* 7  */
    _iocs_tpalet2(COL_LGRAY, 0xCCE);    /* 8  */
    _iocs_tpalet2(COL_DGRAY, 0x445);    /* 9  */
    _iocs_tpalet2(COL_DESKTOP, 0x004);  /* 10 */
    _iocs_tpalet2(COL_TITLE_HI, 0x22F); /* 11 */
    _iocs_tpalet2(COL_ORANGE, 0xFA0);   /* 12 */
    _iocs_tpalet2(COL_TEAL, 0x0AA);     /* 13 */
    _iocs_tpalet2(COL_PURPLE, 0xA0F);   /* 14 */
}

/* ===========================================================
 * V-sync
 * =========================================================== */

void gui_wait_vsync(void) {
    volatile uint8_t* gpip = (volatile uint8_t*)0xE88001;
    while (!(*gpip & 0x10))
        ; /* wait for display period */
    while (*gpip & 0x10)
        ; /* wait for vertical blank (V-sync) */
}

/* ===========================================================
 * Drawing primitives (all to back buffer — fast, in RAM)
 * =========================================================== */

void gui_clear(uint8_t color) { memset(backbuf, color, sizeof(backbuf)); }

void gui_fill_rect(int x, int y, int w, int h, uint8_t color) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > SCREEN_W)
        w = SCREEN_W - x;
    if (y + h > SCREEN_H)
        h = SCREEN_H - y;
    if (w <= 0 || h <= 0)
        return;

    for (int row = y; row < y + h; row++)
        memset(&backbuf[row][x], color, w);
}

void gui_draw_rect(int x, int y, int w, int h, uint8_t color) {
    gui_fill_rect(x, y, w, 1, color);         /* top    */
    gui_fill_rect(x, y + h - 1, w, 1, color); /* bottom */
    gui_fill_rect(x, y, 1, h, color);         /* left   */
    gui_fill_rect(x + w - 1, y, 1, h, color); /* right  */
}

void gui_draw_dotted_rect_xor(int x, int y, int w, int h, int phase) {
    uint8_t mask = 0x07;
    /* Draw horizontal dotted lines (top/bottom) */
    for (int i = 0; i < w; i++) {
        if (((i + phase) % 4) < 2) {
            if (x + i >= 0 && x + i < SCREEN_W) {
                if (y >= 0 && y < SCREEN_H)
                    backbuf[y][x + i] ^= mask;
                if (y + h - 1 >= 0 && y + h - 1 < SCREEN_H)
                    backbuf[y + h - 1][x + i] ^= mask;
            }
        }
    }
    /* Draw vertical dotted lines (left/right) */
    for (int i = 0; i < h; i++) {
        if (((i + phase) % 4) < 2) {
            if (y + i >= 0 && y + i < SCREEN_H) {
                if (x >= 0 && x < SCREEN_W)
                    backbuf[y + i][x] ^= mask;
                if (x + w - 1 >= 0 && x + w - 1 < SCREEN_W)
                    backbuf[y + i][x + w - 1] ^= mask;
            }
        }
    }
}

void gui_draw_cursor_xor(int x, int y) {
    uint8_t mask = 0x07;
    /* Horizontal line (11px wide) */
    for (int i = -5; i <= 5; i++) {
        int px = x + i;
        if (px >= 0 && px < SCREEN_W) {
            if (y >= 0 && y < SCREEN_H)
                backbuf[y][px] ^= mask;
        }
    }
    /* Vertical line (11px tall) */
    for (int i = -5; i <= 5; i++) {
        int py = y + i;
        if (py >= 0 && py < SCREEN_H) {
            if (x >= 0 && x < SCREEN_W)
                backbuf[py][x] ^= mask;
        }
    }
}

void gui_draw_app_windows(void) {
    /* Draw windows from bottom to top without clearing desktop */
    for (int i = 0; i < g_num_wins; i++) {
        int idx = g_zorder[i];
        AppWin* aw = &g_wins[idx];

        /* Draw the window frame */
        gui_draw_window(&aw->win);

        /* Draw body text lines */
        int tx = aw->win.x + 10;
        int ty = aw->win.y + TITLEBAR_H + 8;
        gui_draw_text(tx, ty, aw->line1, COL_WHITE, aw->win.body_bg);
        gui_draw_text(tx, ty + 18, aw->line2, COL_WHITE, aw->win.body_bg);

        aw->dirty = 0;
    }
}

/* Draw one 8×16 character from CGROM */
void gui_draw_char(int x, int y, char c, uint8_t fg, uint8_t bg) {
    const uint8_t* glyph = CGROM_BASE + (uint8_t)c * 16;
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            int px = x + col, py = y + row;
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H)
                backbuf[py][px] = (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

void gui_draw_text(int x, int y, const char* str, uint8_t fg, uint8_t bg) {
    while (*str) {
        gui_draw_char(x, y, *str, fg, bg);
        x += 8;
        str++;
    }
}

/* Draw a window: shadow → border → title bar → body → text */
void gui_draw_window(const Window* win) {
    /* Drop shadow */
    gui_fill_rect(win->x + SHADOW_OFFS, win->y + SHADOW_OFFS, win->w, win->h,
                  COL_DGRAY);
    /* Border */
    gui_draw_rect(win->x, win->y, win->w, win->h, win->border_color);
    /* Title bar */
    gui_fill_rect(win->x + 1, win->y + 1, win->w - 2, 16, win->title_bg);
    gui_draw_text(win->x + 6, win->y + 1, win->title, win->title_fg,
                  win->title_bg);
    /* Body */
    gui_fill_rect(win->x + 1, win->y + TITLEBAR_H, win->w - 2,
                  win->h - TITLEBAR_H - 1, win->body_bg);
}

/* ===========================================================
 * Software mouse cursor (crosshair)
 * =========================================================== */

void gui_draw_cursor(int x, int y) {
    /* Horizontal line (11px wide) */
    for (int i = -5; i <= 5; i++) {
        int px = x + i;
        if (px >= 0 && px < SCREEN_W) {
            if (y >= 0 && y < SCREEN_H)
                backbuf[y][px] = COL_WHITE;
        }
    }
    /* Vertical line (11px tall) */
    for (int i = -5; i <= 5; i++) {
        int py = y + i;
        if (py >= 0 && py < SCREEN_H) {
            if (x >= 0 && x < SCREEN_W)
                backbuf[py][x] = COL_WHITE;
        }
    }
}

/* ===========================================================
 * Multi-window management
 * =========================================================== */

/* Draw all windows in z-order (bottom to top), then the cursor */
void gui_draw_all_windows(void) {
    /* Clear the desktop */
    gui_clear(COL_DESKTOP);

    /* Draw windows from bottom to top */
    for (int i = 0; i < g_num_wins; i++) {
        int idx = g_zorder[i];
        AppWin* aw = &g_wins[idx];

        /* Draw the window frame */
        gui_draw_window(&aw->win);

        /* Draw body text lines */
        int tx = aw->win.x + 10;
        int ty = aw->win.y + TITLEBAR_H + 8;
        gui_draw_text(tx, ty, aw->line1, COL_WHITE, aw->win.body_bg);
        gui_draw_text(tx, ty + 18, aw->line2, COL_WHITE, aw->win.body_bg);

        aw->dirty = 0;
    }
}

/* Mark a specific window's region as needing redraw */
void gui_mark_window_dirty(int idx) {
    if (idx >= 0 && idx < g_num_wins)
        g_wins[idx].dirty = 1;
}

/* Hit-test: which window is at (mx, my)? Returns window index or -1.
 * Searches from top of z-order (front) to bottom. */
int gui_hit_test_window(int mx, int my) {
    for (int i = g_num_wins - 1; i >= 0; i--) {
        int idx = g_zorder[i];
        Window* w = &g_wins[idx].win;
        if (mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h) {
            return idx;
        }
    }
    return -1;
}

/* Bring a window to the top of the z-order */
void gui_bring_to_top(int idx) {
    if (idx < 0 || idx >= g_num_wins)
        return;

    /* Find where idx is in z-order */
    int pos = -1;
    for (int i = 0; i < g_num_wins; i++) {
        if (g_zorder[i] == idx) {
            pos = i;
            break;
        }
    }
    if (pos < 0 || pos == g_num_wins - 1)
        return; /* already at top or not found */

    /* Shift everything below it down one slot */
    for (int i = pos; i < g_num_wins - 1; i++)
        g_zorder[i] = g_zorder[i + 1];
    g_zorder[g_num_wins - 1] = idx;
}

/* Register a window and return its index.
 * Call this during initialization, before the main loop. */
int gui_register_window(const Window* w) {
    if (g_num_wins >= MAX_WINDOWS)
        return -1;
    int idx = g_num_wins;
    g_wins[idx].win = *w;
    g_wins[idx].line1[0] = '\0';
    g_wins[idx].line2[0] = '\0';
    g_wins[idx].dirty = 1;
    g_zorder[idx] = idx;
    g_num_wins++;
    return idx;
}

/* ===========================================================
 * VRAM transfer (back buffer → display)
 * =========================================================== */

void gui_flip_rect(int x, int y, int w, int h) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > SCREEN_W)
        w = SCREEN_W - x;
    if (y + h > SCREEN_H)
        h = SCREEN_H - y;
    if (w <= 0 || h <= 0)
        return;

    volatile uint16_t* vram = VRAM_BASE;
    for (int row = y; row < y + h; row++) {
        for (int col = x; col < x + w; col++)
            vram[row * VRAM_W + col] = backbuf[row][col];
    }
}

void gui_flip_full(void) {
    volatile uint16_t* vram = VRAM_BASE;
    for (int row = 0; row < SCREEN_H; row++) {
        for (int col = 0; col < SCREEN_W; col++)
            vram[row * VRAM_W + col] = backbuf[row][col];
    }
}

/* Flip the dirty region (union of all changed window areas + cursor).
 * For simplicity, just flip the whole screen on changes. */
void gui_flip_dirty(int mx, int my, int mx_prev, int my_prev) {
    (void)mx;
    (void)my;
    (void)mx_prev;
    (void)my_prev;
    gui_flip_full();
}

/* ===========================================================
 * Dirty region tracking
 *
 * Instead of flipping the ENTIRE 384KB to VRAM every frame,
 * we track a bounding rectangle of all changed areas and only
 * transfer that region. During drag this reduces VRAM writes
 * from ~393K pixels to ~50-100K — a 4-8x speedup.
 * =========================================================== */

static int dirty_x0, dirty_y0, dirty_x1, dirty_y1;
static int dirty_active = 0;

void dirty_reset(void) {
    dirty_active = 0;
    dirty_x0 = SCREEN_W;
    dirty_y0 = SCREEN_H;
    dirty_x1 = 0;
    dirty_y1 = 0;
}

void dirty_include(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0)
        return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    if (x1 > SCREEN_W)
        x1 = SCREEN_W;
    int y1 = y + h;
    if (y1 > SCREEN_H)
        y1 = SCREEN_H;

    if (!dirty_active) {
        dirty_x0 = x0;
        dirty_y0 = y0;
        dirty_x1 = x1;
        dirty_y1 = y1;
        dirty_active = 1;
    } else {
        if (x0 < dirty_x0)
            dirty_x0 = x0;
        if (y0 < dirty_y0)
            dirty_y0 = y0;
        if (x1 > dirty_x1)
            dirty_x1 = x1;
        if (y1 > dirty_y1)
            dirty_y1 = y1;
    }
}

void dirty_include_window(const Window* w) {
    /* Include shadow area too */
    dirty_include(w->x, w->y, w->w + SHADOW_OFFS, w->h + SHADOW_OFFS);
}

void dirty_include_cursor(int cx, int cy) {
    dirty_include(cx - 5, cy - 5, 11, 11);
}

int dirty_is_empty(void) { return !dirty_active; }

/* Transfer only the dirty region to VRAM */
void gui_flip_dirty_region(void) {
    if (!dirty_active)
        return;
    gui_flip_rect(dirty_x0, dirty_y0, dirty_x1 - dirty_x0, dirty_y1 - dirty_y0);
    dirty_active = 0;
}
