#include "win.h"
#include "../gfx/gfx.h"
#include "../kernel/kernel.h"
#include <string.h>

static SSWindow windows[SS_MAX_WINDOWS];
static uint8_t zmap[SS_ZMAP_W * SS_ZMAP_H];
static uint16_t win_count;

void ss_win_init(void) {
    memset(windows, 0, sizeof(windows));
    memset(zmap, 0xFF, sizeof(zmap));
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
    win_count++;

    ss_enable_interrupts();
    return win->id;
}

void ss_win_destroy(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    ss_disable_interrupts();
    memset(&windows[id - 1], 0, sizeof(SSWindow));
    win_count--;
    ss_enable_interrupts();
}

void ss_win_show(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    windows[id - 1].flags |= SS_WIN_VISIBLE | SS_WIN_DIRTY;
}

void ss_win_hide(uint16_t id) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    windows[id - 1].flags &= ~SS_WIN_VISIBLE;
}

void ss_win_damage(uint16_t id, int x, int y, int w, int h) {
    if (id == 0 || id > SS_MAX_WINDOWS) return;
    SSWindow* win = &windows[id - 1];
    win->flags |= SS_WIN_DIRTY;
    win->dirty_x = x;
    win->dirty_y = y;
    win->dirty_w = w;
    win->dirty_h = h;
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
}

static void rebuild_zmap(void) {
    memset(zmap, 0xFF, sizeof(zmap));

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
                zmap[by * SS_ZMAP_W + bx] = win->z;
            }
        }
    }
}

void ss_win_render_all(void) {
    rebuild_zmap();

    /* Clear draw page */
    ss_gfx_clear(0);

    /* Render visible windows in z-order */
    for (uint16_t z = 0; z < 256; z++) {
        for (uint16_t i = 0; i < SS_MAX_WINDOWS; i++) {
            SSWindow* win = &windows[i];
            if (win->id == 0) continue;
            if (win->z != z) continue;
            if (!(win->flags & SS_WIN_VISIBLE)) continue;

            /* Check occlusion via zmap */
            int bx0 = win->x / SS_BLOCK_SIZE;
            int by0 = win->y / SS_BLOCK_SIZE;
            int bx1 = (win->x + win->w) / SS_BLOCK_SIZE;
            int by1 = (win->y + win->h) / SS_BLOCK_SIZE;

            int fully_occluded = 1;
            for (int by = by0; by <= by1 && fully_occluded; by++) {
                for (int bx = bx0; bx <= bx1; bx++) {
                    if (bx >= 0 && bx < SS_ZMAP_W && by >= 0 && by < SS_ZMAP_H) {
                        if (zmap[by * SS_ZMAP_W + bx] <= z) {
                            fully_occluded = 0;
                            break;
                        }
                    }
                }
            }

            if (fully_occluded) continue;

            /* Render the window */
            if (win->render) {
                win->render(win);
            } else {
                /* Default: draw a border rectangle */
                ss_gfx_rect(win->x, win->y, win->w, win->h, 0x000F);
                ss_gfx_rect(win->x + 1, win->y + 1, win->w - 2, win->h - 2, 0x0000);
            }

            win->flags &= ~SS_WIN_DIRTY;
        }
    }

    /* Flip the display */
    ss_gfx_flip();
}
