#include "win.h"
#include "../gfx/gfx.h"
#include "../gfx/profile.h"
#include "../kernel/kernel.h"
#include <string.h>

static SSWindow windows[SS_MAX_WINDOWS];
static uint8_t zmap[SS_ZMAP_W * SS_ZMAP_H];
/* The z-map depends only on visibility, geometry, and z-order.  Content
 * updates and repeated partial paints must not pay to rebuild it. */
static uint8_t zmap_valid;
static uint16_t win_count;
uint16_t ss_win_active_z = 0;  /* highest visible z, set by render_all */

void ss_win_init(void) {
    memset(windows, 0, sizeof(windows));
    memset(zmap, 0xFF, sizeof(zmap));
    zmap_valid = 0;
    win_count = 0;
}

uint16_t ss_win_create(int x, int y, int w, int h, uint16_t z) {
    ss_disable_interrupts();

    uint16_t i;
    for (i = 0; i < SS_MAX_WINDOWS; i++) {
        if (windows[i].id == 0) break;
    }
    if (i >= SS_MAX_WINDOWS) {
        ss_enable_interrupts();
        return 0;
    }

    SSWindow* win = &windows[i];
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->z = z;
    win->flags = SS_WIN_VISIBLE | SS_WIN_DIRTY;
    win->dirty_x = 0;
    win->dirty_y = 0;
    win->dirty_w = w;
    win->dirty_h = h;
    win->render = NULL;
    win->id = i + 1;
    memset(win->title, 0, sizeof(win->title));
    memset(win->content, 0, sizeof(win->content));
    memset(win->content_prev, 0, sizeof(win->content_prev));
    win_count++;
    zmap_valid = 0;
    SS_PROFILE_DIRTY_MARK();
    if (w > 0 && h > 0) SS_PROFILE_DIRTY_AREA((uint32_t)w * (uint32_t)h);

    ss_enable_interrupts();
    return win->id;
}

void ss_win_destroy(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    ss_disable_interrupts();
    memset(&windows[id - 1], 0, sizeof(SSWindow));
    win_count--;
    zmap_valid = 0;
    ss_enable_interrupts();
}

void ss_win_show(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    windows[id - 1].flags |= SS_WIN_VISIBLE | SS_WIN_DIRTY;
    zmap_valid = 0;
    SS_PROFILE_DIRTY_MARK();
    SS_PROFILE_DIRTY_AREA((uint32_t)windows[id - 1].w * (uint32_t)windows[id - 1].h);
}

void ss_win_hide(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    windows[id - 1].flags &= ~SS_WIN_VISIBLE;
    zmap_valid = 0;
}

void ss_win_damage(uint16_t id, int x, int y, int w, int h) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    SSWindow* win = &windows[id - 1];
    win->flags |= SS_WIN_DIRTY;
    win->dirty_x = x;
    win->dirty_y = y;
    win->dirty_w = w;
    win->dirty_h = h;
    SS_PROFILE_DIRTY_MARK();
    if (w > 0 && h > 0) SS_PROFILE_DIRTY_AREA((uint32_t)w * (uint32_t)h);
}

void ss_win_move(uint16_t id, int x, int y) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    SSWindow* win = &windows[id - 1];
    win->x = x;
    win->y = y;
    win->flags |= SS_WIN_DIRTY;
    win->dirty_x = 0;
    win->dirty_y = 0;
    win->dirty_w = win->w;
    win->dirty_h = win->h;
    zmap_valid = 0;
    SS_PROFILE_DIRTY_MARK();
    SS_PROFILE_DIRTY_AREA((uint32_t)win->w * (uint32_t)win->h);
}

static void rebuild_zmap(void) {
    SS_PROFILE_ZMAP_REBUILD();
    /* 0 = uncovered.  Must NOT be 0xFF: the max-z update below only writes
     * when win->z > current, and 0xFF(255) would suppress every z<=255. */
    memset(zmap, 0, sizeof(zmap));

    /* Sort windows by z-order (ascending) and update zmap */
    for (uint16_t i = 0; i < SS_MAX_WINDOWS; i++) {
        SSWindow* win = &windows[i];
        if (win->id == 0 || !(win->flags & SS_WIN_VISIBLE)) continue;

        int bx0 = win->x / SS_BLOCK_SIZE;
        int by0 = win->y / SS_BLOCK_SIZE;
        int bx1 = (win->x + win->w) / SS_BLOCK_SIZE;
        int by1 = (win->y + win->h) / SS_BLOCK_SIZE;

        if (bx0 < 0) bx0 = 0;
        if (by0 < 0) by0 = 0;
        if (bx1 >= SS_ZMAP_W) bx1 = SS_ZMAP_W - 1;
        if (by1 >= SS_ZMAP_H) by1 = SS_ZMAP_H - 1;

        for (int by = by0; by <= by1; by++) {
            for (int bx = bx0; bx <= bx1; bx++) {
                /* keep the HIGHEST z covering this block, not just the
                 * last one written (array order is arbitrary vs. z).  The
                 * occlusion test in render_all compares zmap <= win->z, so
                 * max-z here is what makes it correct. */
                if (win->z > zmap[by * SS_ZMAP_W + bx])
                    zmap[by * SS_ZMAP_W + bx] = win->z;
            }
        }
    }
    zmap_valid = 1;
}

static void ensure_zmap(void) {
    if (!zmap_valid) rebuild_zmap();
}

static void draw_frame_rect(SSGfxRect rect, const SSGfxRect* clip, uint16_t color) {
    if (clip == NULL) {
        ss_gfx_rect(rect.x, rect.y, rect.w, rect.h, color);
    } else {
        ss_gfx_rect_region(rect, clip, color);
    }
}

static void draw_frame(SSWindow* win, int is_fg, const SSGfxRect* clip) {
    int th = 12; /* title bar height */
    uint16_t t_bg = is_fg ? 8 : 7;  /* fg: dark gray(8), bg: white(7) */

    /* Title bar */
    draw_frame_rect((SSGfxRect){win->x + 1, win->y + 1, win->w - 2, th - 2},
                    clip, t_bg);
    /* Content area */
    draw_frame_rect((SSGfxRect){win->x + 1, win->y + th, win->w - 2,
                               win->h - th - 1}, clip, 7);
    /* Outer border */
    draw_frame_rect((SSGfxRect){win->x, win->y, win->w, 1}, clip, 0);
    draw_frame_rect((SSGfxRect){win->x, win->y + win->h - 1, win->w, 1},
                    clip, 0);
    draw_frame_rect((SSGfxRect){win->x, win->y, 1, win->h}, clip, 0);
    draw_frame_rect((SSGfxRect){win->x + win->w - 1, win->y, 1, win->h},
                    clip, 0);
    /* Title separator */
    draw_frame_rect((SSGfxRect){win->x + 1, win->y + th - 1, win->w - 2, 1},
                    clip, 0);
}

/*
 * Paint visible windows in ascending z-order, optionally restricted to those
 * overlapping [rx,ry,rw,rh] (region pass-through from the drag path).  The
 * caller has already laid down the stipple background and rebuild_zmap().
 *
 * Replaces the earlier "for z in 0..255" sweep: with <= SS_MAX_WINDOWS
 * windows the z*windows product was pure waste.  We collect visible windows
 * into a small stack array, insertion-sort by z, and render in one pass.
 */
static void paint_windows_zorder(int highest_z,
                                 int rx, int ry, int rw, int rh,
                                 int use_region, const SSGfxRect* clip) {
    SSWindow* order[SS_MAX_WINDOWS];
    int n = 0;

    for (int i = 0; i < SS_MAX_WINDOWS; i++) {
        SSWindow* win = &windows[i];
        if (win->id == 0 || !(win->flags & SS_WIN_VISIBLE)) continue;
        SS_PROFILE_WINDOW_CONSIDERED();
        if (use_region) {
            /* skip windows that don't overlap the dirty region */
            if (win->x >= rx + rw || win->x + (int)win->w <= rx ||
                win->y >= ry + rh || win->y + (int)win->h <= ry)
                { SS_PROFILE_WINDOW_SKIP_NO_OVERLAP(); continue; }
        }
        /* insertion sort: keep order[] ascending by z */
        int j = n;
        while (j > 0 && (int)order[j - 1]->z > (int)win->z) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = win;
        n++;
    }

    for (int k = 0; k < n; k++) {
        SSWindow* win = order[k];

        /* Do not use the coarse 8x8 max-z map as a draw skip.  A block can
         * contain both exposed pixels of this window and pixels covered by a
         * higher window, so max-z cannot prove full rectangle occlusion.
         * Recompose every overlapping window in z-order; with the small
         * window limit this is cheaper than repairing stale pixels after a
         * drag and is the correctness baseline for a future exact occluder. */

        if (win->render) {
            win->render(win, clip);
        } else {
            draw_frame(win, (int)win->z == highest_z, clip);
        }
        SS_PROFILE_WINDOW_RENDERED();
        win->flags &= ~SS_WIN_DIRTY;
    }
}

/* Highest z among visible windows, or -1 if none. */
static int compute_highest_z(void) {
    int highest_z = -1;
    for (int i = 0; i < SS_MAX_WINDOWS; i++) {
        if (windows[i].id && (windows[i].flags & SS_WIN_VISIBLE) &&
            (int)windows[i].z > highest_z)
            highest_z = windows[i].z;
    }
    return highest_z;
}

void ss_win_render_all(void) {
    SS_PROFILE_RENDER_ALL();
    ensure_zmap();

    /* Background stipple (no pre-clear — covers old window positions naturally) */
    ss_gfx_fill_stipple(0, 0, ss_current_mode->display_w,
                        ss_current_mode->display_h, 7, 15);
    SS_PROFILE_FULL_BG_FILL();

    int highest_z = compute_highest_z();
    ss_win_active_z = (uint16_t)highest_z;

    paint_windows_zorder(highest_z, 0, 0, 0, 0, 0, NULL);
}

/*
 * Repaint only a rectangular region (dirty region): stipple its background
 * and redraw visible windows that overlap it, in z-order.  Used by the
 * drag drop path to restore the vacated old position without a full
 * screen repaint.
 */
void ss_win_render_region(int rx, int ry, int rw, int rh) {
    SS_PROFILE_RENDER_REGION();
    ensure_zmap();
    ss_gfx_fill_stipple(rx, ry, rw, rh, 7, 15);
    if (rw > 0 && rh > 0) {
        SS_PROFILE_DIRTY_MARK();
        SS_PROFILE_DIRTY_AREA((uint32_t)rw * (uint32_t)rh);
#if SS_PROFILE_GFX
        int cx = rx, cy = ry, cw = rw, ch = rh;
        if (cx < 0) { cw += cx; cx = 0; }
        if (cy < 0) { ch += cy; cy = 0; }
        if (cx + cw > ss_current_mode->display_w) cw = ss_current_mode->display_w - cx;
        if (cy + ch > ss_current_mode->display_h) ch = ss_current_mode->display_h - cy;
        if (cw > 0 && ch > 0)
            SS_PROFILE_DIRTY_CLIPPED_AREA((uint32_t)cw * (uint32_t)ch);
#endif
    }

    int highest_z = compute_highest_z();
    ss_win_active_z = (uint16_t)highest_z;

    SSGfxRect clip = {rx, ry, rw, rh};
    paint_windows_zorder(highest_z, rx, ry, rw, rh, 1, &clip);
}

int ss_win_hit_test(int mx, int my) {
    int best_z = -1, best_id = -1;
    for (int i = 0; i < SS_MAX_WINDOWS; i++) {
        SSWindow* w = &windows[i];
        if (w->id == 0 || !(w->flags & SS_WIN_VISIBLE)) continue;
        if (mx >= w->x && mx < w->x + (int)w->w &&
            my >= w->y && my < w->y + (int)w->h &&
            (int)w->z > best_z) {
            best_z = w->z;
            best_id = w->id;
        }
    }
    return best_id;
}

int ss_win_get_x(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return 0;
    return windows[id - 1].x;
}

int ss_win_get_y(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return 0;
    return windows[id - 1].y;
}
int ss_win_get_w(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return 0;
    return windows[id - 1].w;
}
int ss_win_get_h(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return 0;
    return windows[id - 1].h;
}
int ss_win_get_z(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return 0;
    return windows[id - 1].z;
}

SSWindow* ss_win_get_ptr(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return NULL;
    return &windows[id - 1];
}

void ss_win_set_title(uint16_t id, const char* title) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    SSWindow* win = &windows[id - 1];
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = '\0';
}

void ss_win_set_content_line(uint16_t id, int line, const char* text) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    if (line < 0 || line >= 3) return;
    SSWindow* win = &windows[id - 1];
    /* Guard the copy: preemptive Timer D ISR can preempt a main-thread
     * read mid-strncpy. Cooperative never preempts a strncpy, but the
     * cost of disable/enable here is negligible, so guard unconditionally
     * for correctness under both threading models. */
    ss_disable_interrupts();
    /* Copy then space-pad to the full buffer width. Padding makes every
     * byte deterministic so callers can do a differential redraw (only
     * repainting the changed suffix) without leaving stale trailing
     * characters when a value shrinks. */
    char* dst = win->content[line];
    size_t cap = sizeof(win->content[line]);
    strncpy(dst, text, cap - 1);
    dst[cap - 1] = '\0';
    for (size_t i = strlen(dst); i < cap - 1; i++) dst[i] = ' ';
    dst[cap - 1] = '\0';
    ss_enable_interrupts();
}

void ss_win_set_render(uint16_t id, void (*render)(SSWindow*, const SSGfxRect*)) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    windows[id - 1].render = render;
}

void ss_win_set_z(uint16_t id, uint16_t z) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    windows[id - 1].z = z;
    zmap_valid = 0;
}

void ss_win_mark_dirty(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    SSWindow* win = &windows[id - 1];
    win->flags |= SS_WIN_DIRTY;
    win->dirty_x = 0;
    win->dirty_y = 0;
    win->dirty_w = win->w;
    win->dirty_h = win->h;
    SS_PROFILE_DIRTY_MARK();
    SS_PROFILE_DIRTY_AREA((uint32_t)win->w * (uint32_t)win->h);
}
