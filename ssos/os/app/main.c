#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include "../kernel/work_queue.h"
#include "../mem/memory.h"
#include "../gfx/gfx.h"
#include "../win/win.h"
#include "../ipc/ipc.h"
#include "../util/numfmt.h"
#include <stdint.h>
#include <string.h>
#include <x68k/iocs.h>

uint8_t* ss_task_stack_base;

/* Drag state */
static int drag_id = -1;
static int drag_ox, drag_oy;
static int drag_w, drag_h;
static int drag_prev_x = -1, drag_prev_y = -1;
/* Start above the initial window z range (1..3) so the first dragged
 * window can't share a z with an existing window.  Otherwise ss_win_active_z
 * would match BOTH the dragged window AND the pre-existing top window,
 * painting both as active (gray + hash stripes). */
static uint16_t next_z = 4;
static uint32_t frame = 0;   /* marching-ants phase */

/* Previous active window: saved at drag start so we can repaint it on
 * release when its is_fg flips (render_region's small rect won't
 * reach windows that don't overlap the dropped position). */
static int prev_active_valid = 0;
static int prev_active_x, prev_active_y, prev_active_w, prev_active_h;

/* Window layout (standalone-compatible) */
#define TITLE_H   12
#define CONTENT_Y 14
#define LINE_H    10
#define WIN_W     240
#define WIN_H     (CONTENT_Y + 3 * LINE_H + 4)
#define LINE_LEN  28
#define APP_MAX_WINS 4
#define OL_MAX     1200

/* 16-color palette indices (set up by premain's palette block). Naming the
 * literals keeps the rendering code readable instead of sprinkling magic
 * numbers through every draw call. */
#define PAL_BLACK 0   /* window borders, text foreground, marching ants */
#define PAL_WHITE 7   /* content background, inactive title bar          */
#define PAL_GRAY  8   /* active (focused) title bar                      */

typedef struct {
    char title[20];
    char line[3][30];
    char prev[3][30];
} WinContent;
static WinContent win_content[APP_MAX_WINS];

/* Outline border save/restore (under the marching-ants outline) */
static uint16_t ol_buf[OL_MAX];
static int ol_x, ol_y, ol_w, ol_h, ol_valid = 0;

static void wait_vsync(void) {
    uint32_t last = ss_vsync_counter;
    while (ss_vsync_counter == last);
}

static int cur_mx = 0, cur_my = 0, cur_btn = 0;
static void update_mouse(void) {
    int pos, dt;
    asm volatile(
        "moveq #0x75, %%d0\n\t"   "trap #15\n\t"   "move.l %%d0, %0\n\t"
        "moveq #0x74, %%d0\n\t"   "trap #15\n\t"   "move.l %%d0, %1"
        : "=d"(pos), "=d"(dt) :: "d0"
    );
    cur_mx = (int16_t)((pos >> 16) & 0xFFFF);
    cur_my = (int16_t)(pos & 0xFFFF);
    cur_btn = dt;
}

static int last_key = -1;
static void update_keyboard(void) {
    if (_iocs_b_keysns() > 0) {
        last_key = _iocs_b_keyinp();
    }
}

/* XOR outline for the software cursor (6x6) */
static int cur_prev_x = -100, cur_prev_y = -100;
static void xor_outline(int x, int y, int w, int h) {
    uint32_t stride = ss_current_mode->bytes_per_line / 2;
    int W = ss_current_mode->display_w, H = ss_current_mode->display_h;
    volatile uint16_t* v = ss_draw_page;
    for (int dx = 0; dx < w; dx++) {
        int xx = x + dx;
        if (xx >= 0 && xx < W) {
            if (y >= 0 && y < H) v[y * stride + xx] ^= 0xFFFF;
            int y2 = y + h - 1;
            if (y2 >= 0 && y2 < H) v[y2 * stride + xx] ^= 0xFFFF;
        }
    }
    for (int dy = 0; dy < h; dy++) {
        int yy = y + dy;
        if (yy >= 0 && yy < H) {
            if (x >= 0 && x < W) v[yy * stride + x] ^= 0xFFFF;
            int x2 = x + w - 1;
            if (x2 >= 0 && x2 < W) v[yy * stride + x2] ^= 0xFFFF;
        }
    }
}

static void pad_line(char* s, int n) {
    int l = (int)strlen(s);
    for (int i = l; i < n; i++) s[i] = ' ';
    s[n] = '\0';
}

static void draw_content_dirty(uint16_t id) {
    if (id == 0 || id > APP_MAX_WINS) return;
    int x = ss_win_get_x(id), y = ss_win_get_y(id);
    WinContent* c = &win_content[id - 1];
    for (int i = 0; i < 3; i++) {
        if (memcmp(c->line[i], c->prev[i], 30) != 0) {
            /* Redraw only the changed suffix: lines are pad_line'd to
             * LINE_LEN and left-aligned, so the first differing column
             * is where the visible change starts. Redrawing from there
             * to the (space-padded) tail handles both growth and erase.
             * Turns "Vsync: 100" -> "Vsync: 101" into a 1-char repaint. */
            int j = 0;
            while (j < LINE_LEN && c->line[i][j] == c->prev[i][j]) j++;
            ss_gfx_draw_text_fast(x + 4 + j * SS_FONT_ADV,
                                  y + CONTENT_Y + i * LINE_H,
                                  c->line[i] + j, PAL_BLACK, PAL_WHITE);
            memcpy(c->prev[i], c->line[i], 30);
        }
    }
}

/* Save/restore only the border pixels (under the marching-ants outline). */
static void ol_save(int x, int y, int w, int h) {
    int W = ss_current_mode->display_w, H = ss_current_mode->display_h;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;
    if (w <= 0 || h <= 0 || w * 2 + h * 2 > OL_MAX) { ol_valid = 0; return; }
    ol_x = x; ol_y = y; ol_w = w; ol_h = h;
    uint32_t stride = ss_current_mode->bytes_per_line / 2;
    int idx = 0;
    for (int i = 0; i < w && idx < OL_MAX; i++, idx++)
        ol_buf[idx] = ss_draw_page[y * stride + x + i];
    for (int i = 0; i < w && idx < OL_MAX; i++, idx++)
        ol_buf[idx] = ss_draw_page[(uint32_t)(y + h - 1) * stride + x + i];
    for (int i = 1; i < h - 1 && idx < OL_MAX; i++, idx++)
        ol_buf[idx] = ss_draw_page[(uint32_t)(y + i) * stride + x];
    for (int i = 1; i < h - 1 && idx < OL_MAX; i++, idx++)
        ol_buf[idx] = ss_draw_page[(uint32_t)(y + i) * stride + x + w - 1];
    ol_valid = 1;
}
static void ol_restore(void) {
    if (!ol_valid) return;
    int x = ol_x, y = ol_y, w = ol_w, h = ol_h;
    uint32_t stride = ss_current_mode->bytes_per_line / 2;
    int idx = 0;
    for (int i = 0; i < w && idx < OL_MAX; i++, idx++)
        ss_draw_page[y * stride + x + i] = ol_buf[idx];
    for (int i = 0; i < w && idx < OL_MAX; i++, idx++)
        ss_draw_page[(uint32_t)(y + h - 1) * stride + x + i] = ol_buf[idx];
    for (int i = 1; i < h - 1 && idx < OL_MAX; i++, idx++)
        ss_draw_page[(uint32_t)(y + i) * stride + x] = ol_buf[idx];
    for (int i = 1; i < h - 1 && idx < OL_MAX; i++, idx++)
        ss_draw_page[(uint32_t)(y + i) * stride + x + w - 1] = ol_buf[idx];
    ol_valid = 0;
}

/* Marching ants outline (standalone draw_march_outline). */
static void draw_march_outline(int x, int y, int w, int h) {
    uint16_t colors[2] = {PAL_WHITE, PAL_BLACK};
    int offset = (int)(frame & 7);
    uint32_t stride = ss_current_mode->bytes_per_line / 2;
    int W = ss_current_mode->display_w, H = ss_current_mode->display_h;
    volatile uint16_t* v = ss_draw_page;
    int peri = 0;
    for (int i = 0; i < w; i++, peri++) {
        int px = x + i;
        if (px >= 0 && px < W && y >= 0 && y < H) v[y * stride + px] = colors[((peri + offset) >> 2) & 1];
    }
    for (int i = 1; i < h; i++, peri++) {
        int py = y + i, px = x + w - 1;
        if (px >= 0 && px < W && py >= 0 && py < H) v[py * stride + px] = colors[((peri + offset) >> 2) & 1];
    }
    for (int i = w - 2; i >= 0; i--, peri++) {
        int px = x + i, py = y + h - 1;
        if (px >= 0 && px < W && py >= 0 && py < H) v[py * stride + px] = colors[((peri + offset) >> 2) & 1];
    }
    for (int i = h - 2; i >= 1; i--, peri++) {
        int py = y + i;
        if (x >= 0 && x < W && py >= 0 && py < H) v[py * stride + x] = colors[((peri + offset) >> 2) & 1];
    }
}

/*
 * Window render callback: standalone-style frame + content.
 * Active (topmost) window: gray title bar with black hash stripes flanking
 * the title (matches standalone draw_frame).  Inactive: plain white bar.
 */
static void render_win(SSWindow* self) {
    if (self->id == 0 || self->id > APP_MAX_WINS) return;
    WinContent* c = &win_content[self->id - 1];
    int x = self->x, y = self->y, w = self->w, h = self->h;
    int is_fg = (self->z == ss_win_active_z);
    uint16_t t_bg = is_fg ? PAL_GRAY : PAL_WHITE;

    ss_gfx_rect(x + 1, y + 1, w - 2, TITLE_H - 2, t_bg);
    ss_gfx_rect(x + 1, y + TITLE_H, w - 2, h - TITLE_H - 1, PAL_WHITE);
    ss_gfx_rect(x, y, w, 1, PAL_BLACK);
    ss_gfx_rect(x, y + h - 1, w, 1, PAL_BLACK);
    ss_gfx_rect(x, y, 1, h, PAL_BLACK);
    ss_gfx_rect(x + w - 1, y, 1, h, PAL_BLACK);
    ss_gfx_rect(x + 1, y + TITLE_H - 1, w - 2, 1, PAL_BLACK);

    int tw = (int)strlen(c->title) * SS_FONT_ADV;
    int tx = x + (w - tw) / 2;
    if (is_fg) {
        /* black hash stripes on both sides of the title (standalone look) */
        for (int i = 0; i < 5; i++) {
            int ly = y + 2 + i * 2;
            if (tx > x + 12)
                ss_gfx_rect(x + 4, ly, tx - 8 - (x + 4) + 1, 1, PAL_BLACK);
            if (tx + tw + 8 < x + w - 4)
                ss_gfx_rect(tx + tw + 8, ly, (x + w - 5) - (tx + tw + 8) + 1, 1, PAL_BLACK);
        }
    }
    ss_gfx_draw_text_fast(tx, y + 2, c->title, PAL_BLACK, t_bg);

    for (int i = 0; i < 3; i++)
        ss_gfx_draw_text_fast(x + 4, y + CONTENT_Y + i * LINE_H, c->line[i], PAL_BLACK, PAL_WHITE);
    memcpy(c->prev[0], c->line[0], 30);
    memcpy(c->prev[1], c->line[1], 30);
    memcpy(c->prev[2], c->line[2], 30);
}

void ss_init(void) {
    ss_mem_init((void*)&__ssosram_start, (uintptr_t)&__ssosram_size);
    ss_task_stack_base = (uint8_t*)ss_alloc(SS_MAX_TASKS * SS_TASK_STACK);
    ss_sched_init();
    ss_work_init(&ss_main_work_queue);
    ss_ipc_init();
    ss_gfx_init();
    ss_win_init();
}

static void update_content(uint16_t wt, uint16_t wk, uint16_t wm,
                           int mx, int my, int left, int right) {
    char* p;
    WinContent* t = &win_content[wt - 1];
    /* Avoid sprintf on the hot path: build counter lines directly. */
    p = t->line[0]; memcpy(p, "Vsync: ", 7); ss_utoa_dec(frame, p + 7); pad_line(p, LINE_LEN);
    p = t->line[1]; memcpy(p, "VDisp:", 6); ss_utoa_dec(ss_vdisp_fire_count, p + 6); pad_line(p, LINE_LEN);
    p = t->line[2]; memcpy(p, "Tick: ", 6); ss_utoa_dec(ss_timerd_fire_count, p + 6); pad_line(p, LINE_LEN);

    WinContent* k = &win_content[wk - 1];
    if (last_key >= 0) {
        int code = last_key & 0xFF;
        char ch = (code >= 0x20 && code < 0x7F) ? (char)code : '.';
        p = k->line[0];
        memcpy(p, "Code:", 5);                 /* "Code:" */
        ss_utoa_hex((uint32_t)code, p + 5, 2); /* "XX"    */
        p[7] = 'H'; p[8] = ' '; p[9] = '\''; p[10] = ch; p[11] = '\''; p[12] = '\0';
        pad_line(p, LINE_LEN);
        p = k->line[1];
        memcpy(p, "Shift:", 6);
        ss_utoa_hex((uint32_t)((last_key >> 8) & 0xFF), p + 6, 2);
        p[8] = 'H'; p[9] = '\0';
        pad_line(p, LINE_LEN);
        k->line[2][0] = '\0';
    } else {
        strcpy(k->line[0], "Press any key...");
        k->line[1][0] = '\0';
        k->line[2][0] = '\0';
    }
    pad_line(k->line[0], LINE_LEN); pad_line(k->line[1], LINE_LEN); pad_line(k->line[2], LINE_LEN);

    WinContent* m = &win_content[wm - 1];
    p = m->line[0];
    p[0] = 'X'; p[1] = ':';
    int n = ss_itoa_dec_pad(mx, p + 2, 3);
    int off = 2 + n;
    p[off] = ' '; p[off + 1] = 'Y'; p[off + 2] = ':';
    int n2 = ss_itoa_dec_pad(my, p + off + 3, 3);
    p[off + 3 + n2] = '\0';
    pad_line(p, LINE_LEN);

    p = m->line[1];
    p[0] = 'L'; p[1] = '=';
    p[2] = left ? 'D' : 'U'; p[3] = left ? 'N' : 'P';
    p[4] = ' '; p[5] = 'R'; p[6] = '=';
    p[7] = right ? 'D' : 'U'; p[8] = right ? 'N' : 'P';
    p[9] = '\0';
    pad_line(p, LINE_LEN);
    m->line[2][0] = '\0';
}

void ss_run(void) {
    uint16_t w_timer = ss_win_create(30, 15, WIN_W, WIN_H, 1);
    uint16_t w_key   = ss_win_create(180, 60, WIN_W, WIN_H, 2);
    uint16_t w_mouse = ss_win_create(80, 120, WIN_W, WIN_H, 3);
    ss_win_set_render(w_timer, render_win);
    ss_win_set_render(w_key, render_win);
    ss_win_set_render(w_mouse, render_win);
    strcpy(win_content[w_timer - 1].title, "Timer");
    strcpy(win_content[w_key   - 1].title, "Keyboard");
    strcpy(win_content[w_mouse - 1].title, "Mouse");

    ss_win_render_all();

    while (1) {
        wait_vsync();
        frame++;
        update_mouse();
        update_keyboard();
        int mx = cur_mx, my = cur_my, btn = cur_btn;
        int left = (btn & 0x0200) != 0;
        int right = (btn & 0x0001) != 0;

        update_content(w_timer, w_key, w_mouse, mx, my, left, right);

        /* Erase previous cursor BEFORE any region repaint, so a repaint
         * that overwrites the cursor pixel doesn't leave a stray XOR mark. */
        if (cur_prev_x >= 0) xor_outline(cur_prev_x, cur_prev_y, 6, 6);

        /* Drag (standalone-style: marching ants + region repaint, no full) */
        if (left && drag_id < 0) {
            int hid = ss_win_hit_test(mx, my);
            if (hid > 0) {
                /* Capture the previous active window (z == current active_z)
                 * BEFORE set_z, so we can repaint it on release if it loses
                 * the active title.  Skip if it's the dragged window itself. */
                prev_active_valid = 0;
                for (int i = 1; i <= SS_MAX_WINDOWS; i++) {
                    if (i == hid) continue;
                    if (ss_win_get_z(i) == ss_win_active_z) {
                        prev_active_x = ss_win_get_x(i);
                        prev_active_y = ss_win_get_y(i);
                        prev_active_w = ss_win_get_w(i);
                        prev_active_h = ss_win_get_h(i);
                        prev_active_valid = 1;
                        break;
                    }
                }
                drag_id = hid;
                drag_ox = mx - ss_win_get_x(hid);
                drag_oy = my - ss_win_get_y(hid);
                drag_w  = ss_win_get_w(hid);
                drag_h  = ss_win_get_h(hid);
                ss_win_set_z(hid, next_z);
                if (next_z >= 255) next_z = 3;
                else next_z++;
                int ox = ss_win_get_x(hid), oy = ss_win_get_y(hid);
                ss_win_hide(hid);
                ss_win_render_region(ox, oy, drag_w, drag_h);  /* restore old spot */
                ol_save(ox, oy, drag_w, drag_h);
                draw_march_outline(ox, oy, drag_w, drag_h);
                drag_prev_x = ox; drag_prev_y = oy;
            }
        } else if (left && drag_id > 0) {
            int nx = mx - drag_ox, ny = my - drag_oy;
            int W = ss_current_mode->display_w, H = ss_current_mode->display_h;
            if (nx < 0) nx = 0;
            if (ny < 0) ny = 0;
            if (nx + drag_w > W) nx = W - drag_w;
            if (ny + drag_h > H) ny = H - drag_h;
            if (nx != drag_prev_x || ny != drag_prev_y) {
                ol_restore();
                ol_save(nx, ny, drag_w, drag_h);
                drag_prev_x = nx; drag_prev_y = ny;
            }
            draw_march_outline(drag_prev_x, drag_prev_y, drag_w, drag_h);
        } else if (!left && drag_id > 0) {
            ol_restore();
            ss_win_show(drag_id);
            ss_win_move(drag_id, drag_prev_x, drag_prev_y);
            /* Region repaint at new position: paints the window (now active,
             * since set_z raised it) AND refreshes ss_win_active_z. */
            ss_win_render_region(drag_prev_x, drag_prev_y, drag_w, drag_h);
            /* Repaint the previous active window: render_region only
             * touches windows overlapping the new position, so the
             * window that just lost the active title is left painted
             * in the active color.  Force its repaint now. */
            if (prev_active_valid) {
                ss_win_render_region(prev_active_x, prev_active_y,
                                     prev_active_w, prev_active_h);
            }
            cur_prev_x = -100;
            drag_id = -1;
            drag_prev_x = -1;
            prev_active_valid = 0;
        }

        if (drag_id <= 0) {
            draw_content_dirty(w_timer);
            draw_content_dirty(w_key);
            draw_content_dirty(w_mouse);
        }

        /* Draw cursor (erase happens at the top of the next frame) */
        xor_outline(mx, my, 6, 6);
        cur_prev_x = mx; cur_prev_y = my;

        ss_work_drain(&ss_main_work_queue);
        ss_process_wakeups();
        ss_task_yield();
    }
}
