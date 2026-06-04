/*
 * SSOS Standalone — Preemptive Multithreading Windowed Demo
 * CRT: 512x512 256-color (crtmod 8)
 * Font: Spleen 5x8
 * Exit: ESC key
 *
 * Architecture:
 *   Thread 1 (main, pri=8):   Rendering loop + mouse IOCS (V-sync rate)
 *   Thread 2 (data, pri=8):   Timer/Keyboard window data + IOCS calls
 */
#include "../os/kernel/kernel.h"
#include "../os/kernel/scheduler.h"
#include "../os/mem/memory.h"
#include "../os/gfx/gfx.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

#ifdef LOCAL_MODE

static uint8_t local_memory[512 * 1024] __attribute__((aligned(4)));
static uint8_t local_stack_mem[SS_MAX_TASKS * SS_TASK_STACK]
    __attribute__((aligned(4)));

uint8_t* ss_task_stack_base = local_stack_mem;

extern uint32_t ss_switch_count;

/* Palette Indices */
#define C_WHITE 0
#define C_BLACK 215
#define C_GRAY_L 247
#define C_GRAY_M 250
#define C_GRAY_D 252

#define PAL_RGB(r, g, b)                                                       \
    (uint16_t)((((g) & 0x1F) << 11) | (((r) & 0x1F) << 6) |                    \
               (((b) & 0x1F) << 1) | 1)

static void set_palette(void) {
    int i, r, g, b;
    int idx = 0;
    static const uint8_t cube_levels[] = {31, 25, 18, 12, 6, 0};
    static const uint8_t system_levels[] = {29, 27, 23, 21, 17,
                                             15, 10, 8,  4,  2};

    for (r = 0; r < 6; r++)
        for (g = 0; g < 6; g++)
            for (b = 0; b < 6; b++)
                _iocs_gpalet(idx++, PAL_RGB(cube_levels[r], cube_levels[g],
                                            cube_levels[b]));

    for (i = 0; i < 10; i++)
        _iocs_gpalet(idx++, PAL_RGB(system_levels[i], 0, 0));
    for (i = 0; i < 10; i++)
        _iocs_gpalet(idx++, PAL_RGB(0, system_levels[i], 0));
    for (i = 0; i < 10; i++)
        _iocs_gpalet(idx++, PAL_RGB(0, 0, system_levels[i]));
    for (i = 0; i < 10; i++)
        _iocs_gpalet(idx++, PAL_RGB(system_levels[i], system_levels[i],
                                    system_levels[i]));
}

/* Window layout */
#define TITLE_H 12
#define CONTENT_Y 14
#define LINE_H 10
#define WIN_W 240
#define WIN_H (CONTENT_Y + 3 * LINE_H + 4)

typedef struct {
    int x, y, w, h;
    char title[20];
    char line[3][30];
    char prev[3][30];
} Win;

#define NUM_WINS 3

static Win wins[NUM_WINS];
static int zmap[NUM_WINS] = {0, 1, 2};
static volatile uint32_t frame = 0;
static volatile int last_key = -1;
static volatile int mx = SS_SCREEN_W / 2, my = SS_SCREEN_H / 2;
static volatile uint8_t mb_left, mb_right;
static volatile int exit_flag = 0;
static int drag = -1, dox, doy;
static uint16_t win_save_buf[WIN_W * WIN_H];
static int win_save_valid;
static int need_full = 1;

static uint32_t saved_copy_vec;
static uint32_t saved_nmi_vec;
extern void ss_nop_handler(void);

extern void ss_init_trap14(void);
extern void ss_restore_trap14(void);
extern uint16_t ss_trapbuf_flag;
extern uint16_t ss_trapbuf_sr;
extern uint32_t ss_trapbuf_pc;
extern char* ss_trapbuf_msg;

static SSTask main_tcb;

#define OL_MAX 1200
static uint16_t ol_buf[OL_MAX];
static int ol_x, ol_y, ol_w, ol_h, ol_valid;

/* ---- GVRAM bitmap save/restore (direct page 0 access) ---- */

static void save_win_bitmap(int x, int y, int w, int h) {
    volatile uint16_t* s = ss_draw_page + (uint32_t)y * SS_SCREEN_W + x;
    for (int row = 0; row < h; row++) {
        for (int i = 0; i < w; i++) win_save_buf[row * w + i] = s[i];
        s += SS_SCREEN_W;
    }
    win_save_valid = 1;
}

static void restore_win_bitmap(int x, int y, int w, int h) {
    volatile uint16_t* d = ss_draw_page + (uint32_t)y * SS_SCREEN_W + x;
    for (int row = 0; row < h; row++) {
        uint16_t* src = win_save_buf + row * w;
        if ((x & 1) == 0) {
            volatile uint32_t* dl = (volatile uint32_t*)d;
            uint32_t* sl = (uint32_t*)src;
            for (int i = 0; i < w / 2; i++) dl[i] = sl[i];
            if (w & 1) d[w - 1] = src[w - 1];
        } else {
            for (int i = 0; i < w; i++) d[i] = src[i];
        }
        d += SS_SCREEN_W;
    }
    win_save_valid = 0;
}

/* ---- Window overlap check ---- */

static int win_overlap(int zpos) {
    Win* w = &wins[zmap[zpos]];
    for (int k = zpos + 1; k < NUM_WINS; k++) {
        Win* ow = &wins[zmap[k]];
        if (w->x < ow->x + ow->w && w->x + w->w > ow->x &&
            w->y < ow->y + ow->h && w->y + w->h > ow->y)
            return 1;
    }
    return 0;
}

/* ---- Clipped text (uses OS gfx module) ---- */

static void draw_text_clip(int px, int py, const char* s, uint16_t fg,
                           uint16_t bg, int zpos) {
    if (!win_overlap(zpos)) {
        ss_gfx_draw_text(px, py, s, fg, bg);
        return;
    }
    int clips[NUM_WINS][4];
    for (int i = 0; i < NUM_WINS; i++) {
        Win* w = &wins[zmap[i]];
        clips[i][0] = w->x; clips[i][1] = w->y;
        clips[i][2] = w->w; clips[i][3] = w->h;
    }
    ss_gfx_draw_text_clip(px, py, s, fg, bg, (const int*)clips, NUM_WINS, zpos);
}

/* ---- Window frame drawing ---- */

static void draw_frame(Win* w, int is_fg) {
    uint16_t t_bg = is_fg ? C_GRAY_L : C_WHITE;

    ss_gfx_rect(w->x + 1, w->y + 1, w->w - 2, TITLE_H - 2, t_bg);
    ss_gfx_rect(w->x + 1, w->y + TITLE_H, w->w - 2, w->h - TITLE_H - 1, C_WHITE);

    /* Border lines */
    ss_gfx_rect(w->x, w->y, w->w, 1, C_BLACK);
    ss_gfx_rect(w->x, w->y + w->h - 1, w->w, 1, C_BLACK);
    ss_gfx_rect(w->x, w->y + 1, 1, w->h - 2, C_BLACK);
    ss_gfx_rect(w->x + w->w - 1, w->y + 1, 1, w->h - 2, C_BLACK);
    ss_gfx_rect(w->x + 1, w->y + TITLE_H - 1, w->w - 2, 1, C_BLACK);

    int tw = (int)strlen(w->title) * SS_FONT_ADV;
    int tx = w->x + (w->w - tw) / 2;

    if (is_fg) {
        for (int i = 0; i < 5; i++) {
            int ly = w->y + 2 + i * 2;
            if (tx > w->x + 12)
                ss_gfx_rect(w->x + 4, ly, tx - 8 - (w->x + 4) + 1, 1, C_BLACK);
            if (tx + tw + 8 < w->x + w->w - 4)
                ss_gfx_rect(tx + tw + 8, ly, (w->x + w->w - 5) - (tx + tw + 8) + 1, 1, C_BLACK);
        }
    }

    ss_gfx_draw_text(tx, w->y + 2, w->title, C_BLACK, t_bg);
}

/* ---- Dirty content update ---- */

static void pad(char* s, int n) {
    int l = (int)strlen(s);
    for (int i = l; i < n; i++) s[i] = ' ';
    s[n] = '\0';
}

static void draw_content_dirty(Win* w) {
    for (int i = 0; i < 3; i++) {
        if (memcmp(w->line[i], w->prev[i], 30) != 0) {
            ss_gfx_draw_text(w->x + 4, w->y + CONTENT_Y + i * LINE_H,
                             w->line[i], C_BLACK, C_WHITE);
            memcpy(w->prev[i], w->line[i], 30);
        }
    }
}

/* ---- Marching ants outline ---- */

static void draw_march_outline(int x, int y, int w, int h) {
    uint16_t colors[2] = {C_WHITE, C_BLACK};
    int offset = (int)(frame & 7);
    int peri = 0;

    if (x >= 0 && y >= 0 && x + w <= SS_SCREEN_W && y + h <= SS_SCREEN_H) {
        volatile uint16_t* row;
        row = ss_draw_page + y * (uint32_t)SS_SCREEN_W + x;
        for (int i = 0; i < w; i++, peri++)
            row[i] = colors[((peri + offset) >> 2) & 1];
        for (int i = 1; i < h; i++, peri++)
            ss_draw_page[(uint32_t)(y + i) * SS_SCREEN_W + x + w - 1] =
                colors[((peri + offset) >> 2) & 1];
        row = ss_draw_page + (uint32_t)(y + h - 1) * SS_SCREEN_W + x;
        for (int i = w - 2; i >= 0; i--, peri++)
            row[i] = colors[((peri + offset) >> 2) & 1];
        for (int i = h - 2; i >= 1; i--, peri++)
            ss_draw_page[(uint32_t)(y + i) * SS_SCREEN_W + x] =
                colors[((peri + offset) >> 2) & 1];
    } else {
        for (int i = 0; i < w; i++, peri++) {
            int px = x + i;
            if (px >= 0 && px < SS_SCREEN_W && y >= 0 && y < SS_SCREEN_H)
                ss_draw_page[y * (uint32_t)SS_SCREEN_W + px] =
                    colors[((peri + offset) >> 2) & 1];
        }
        for (int i = 1; i < h; i++, peri++) {
            int py = y + i;
            if (x + w - 1 >= 0 && x + w - 1 < SS_SCREEN_W && py >= 0 && py < SS_SCREEN_H)
                ss_draw_page[py * (uint32_t)SS_SCREEN_W + x + w - 1] =
                    colors[((peri + offset) >> 2) & 1];
        }
        for (int i = w - 2; i >= 0; i--, peri++) {
            int px = x + i;
            if (px >= 0 && px < SS_SCREEN_W && y + h - 1 >= 0 && y + h - 1 < SS_SCREEN_H)
                ss_draw_page[(uint32_t)(y + h - 1) * SS_SCREEN_W + px] =
                    colors[((peri + offset) >> 2) & 1];
        }
        for (int i = h - 2; i >= 1; i--, peri++) {
            int py = y + i;
            if (x >= 0 && x < SS_SCREEN_W && py >= 0 && py < SS_SCREEN_H)
                ss_draw_page[py * (uint32_t)SS_SCREEN_W + x] =
                    colors[((peri + offset) >> 2) & 1];
        }
    }
}

/* ---- Outline save / restore ---- */

static void ol_save(int x, int y, int w, int h) {
    ol_x = x; ol_y = y; ol_w = w; ol_h = h;
    int idx = 0;

    if (x >= 0 && y >= 0 && x + w <= SS_SCREEN_W && y + h <= SS_SCREEN_H) {
        volatile uint16_t* p;
        p = ss_draw_page + y * (uint32_t)SS_SCREEN_W + x;
        for (int i = 0; i < w && idx < OL_MAX; i++, idx++) ol_buf[idx] = p[i];
        p = ss_draw_page + (uint32_t)(y + h - 1) * SS_SCREEN_W + x;
        for (int i = 0; i < w && idx < OL_MAX; i++, idx++) ol_buf[idx] = p[i];
        for (int i = 1; i < h - 1 && idx < OL_MAX; i++, idx++)
            ol_buf[idx] = ss_draw_page[(uint32_t)(y + i) * SS_SCREEN_W + x];
        for (int i = 1; i < h - 1 && idx < OL_MAX; i++, idx++)
            ol_buf[idx] = ss_draw_page[(uint32_t)(y + i) * SS_SCREEN_W + x + w - 1];
    } else {
        for (int i = 0; i < w && idx < OL_MAX; i++) {
            int px = x + i;
            ol_buf[idx++] = (px >= 0 && px < SS_SCREEN_W && y >= 0 && y < SS_SCREEN_H)
                ? ss_draw_page[y * (uint32_t)SS_SCREEN_W + px] : 0;
        }
        for (int i = 0; i < w && idx < OL_MAX; i++) {
            int px = x + i, py = y + h - 1;
            ol_buf[idx++] = (px >= 0 && px < SS_SCREEN_W && py >= 0 && py < SS_SCREEN_H)
                ? ss_draw_page[py * (uint32_t)SS_SCREEN_W + px] : 0;
        }
        for (int i = 1; i < h - 1 && idx < OL_MAX; i++) {
            int py = y + i;
            ol_buf[idx++] = (x >= 0 && x < SS_SCREEN_W && py >= 0 && py < SS_SCREEN_H)
                ? ss_draw_page[py * (uint32_t)SS_SCREEN_W + x] : 0;
        }
        for (int i = 1; i < h - 1 && idx < OL_MAX; i++) {
            int py = y + i, px = x + w - 1;
            ol_buf[idx++] = (px >= 0 && px < SS_SCREEN_W && py >= 0 && py < SS_SCREEN_H)
                ? ss_draw_page[py * (uint32_t)SS_SCREEN_W + px] : 0;
        }
    }
    ol_valid = 1;
}

static void ol_restore(void) {
    if (!ol_valid) return;
    int x = ol_x, y = ol_y, w = ol_w, h = ol_h;
    int idx = 0;

    if (x >= 0 && y >= 0 && x + w <= SS_SCREEN_W && y + h <= SS_SCREEN_H) {
        volatile uint16_t* p;
        p = ss_draw_page + y * (uint32_t)SS_SCREEN_W + x;
        for (int i = 0; i < w && idx < OL_MAX; i++, idx++) p[i] = ol_buf[idx];
        p = ss_draw_page + (uint32_t)(y + h - 1) * SS_SCREEN_W + x;
        for (int i = 0; i < w && idx < OL_MAX; i++, idx++) p[i] = ol_buf[idx];
        for (int i = 1; i < h - 1 && idx < OL_MAX; i++, idx++)
            ss_draw_page[(uint32_t)(y + i) * SS_SCREEN_W + x] = ol_buf[idx];
        for (int i = 1; i < h - 1 && idx < OL_MAX; i++, idx++)
            ss_draw_page[(uint32_t)(y + i) * SS_SCREEN_W + x + w - 1] = ol_buf[idx];
    } else {
        for (int i = 0; i < w && idx < OL_MAX; i++) {
            int px = x + i;
            if (px >= 0 && px < SS_SCREEN_W && y >= 0 && y < SS_SCREEN_H)
                ss_draw_page[y * (uint32_t)SS_SCREEN_W + px] = ol_buf[idx];
            idx++;
        }
        for (int i = 0; i < w && idx < OL_MAX; i++) {
            int px = x + i, py = y + h - 1;
            if (px >= 0 && px < SS_SCREEN_W && py >= 0 && py < SS_SCREEN_H)
                ss_draw_page[py * (uint32_t)SS_SCREEN_W + px] = ol_buf[idx];
            idx++;
        }
        for (int i = 1; i < h - 1 && idx < OL_MAX; i++) {
            int py = y + i;
            if (x >= 0 && x < SS_SCREEN_W && py >= 0 && py < SS_SCREEN_H)
                ss_draw_page[py * (uint32_t)SS_SCREEN_W + x] = ol_buf[idx];
            idx++;
        }
        for (int i = 1; i < h - 1 && idx < OL_MAX; i++) {
            int py = y + i, px = x + w - 1;
            if (px >= 0 && px < SS_SCREEN_W && py >= 0 && py < SS_SCREEN_H)
                ss_draw_page[py * (uint32_t)SS_SCREEN_W + px] = ol_buf[idx];
            idx++;
        }
    }
    ol_valid = 0;
}

/* ---- Z-order ---- */

static void bring_to_front(int idx) {
    int p = -1;
    for (int i = 0; i < NUM_WINS; i++)
        if (zmap[i] == idx) { p = i; break; }
    if (p < 0 || p == NUM_WINS - 1) return;
    for (int i = p; i < NUM_WINS - 1; i++) zmap[i] = zmap[i + 1];
    zmap[NUM_WINS - 1] = idx;
}

static int hit_test(int hx, int hy) {
    for (int i = NUM_WINS - 1; i >= 0; i--) {
        Win* w = &wins[zmap[i]];
        if (hx >= w->x && hx < w->x + w->w && hy >= w->y && hy < w->y + w->h)
            return zmap[i];
    }
    return -1;
}

/* ---- V-sync ---- */

static void wait_vsync(void) {
    volatile uint8_t* gpip = (volatile uint8_t*)0xE88001;
    while (!(*gpip & 0x10));
    while (*gpip & 0x10);
}

/* ================================================================
 * Data Thread — Timer/Keyboard window text + IOCS
 * ================================================================ */

static void* data_thread(void* arg) {
    (void)arg;
    for (;;) {
        if (exit_flag)
            for (;;) ss_task_sleep(0x7FFFFFFF);

        if (_iocs_b_keysns() > 0) {
            int key = _iocs_b_keyinp();
            if ((key & 0xFF) == 0x1B) {
                exit_flag = 1;
            }
            last_key = key;
        }
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

        uint32_t f = frame;
        uint32_t sec = f / 55;
        uint32_t frac = (f % 55) * 100 / 55;
        sprintf(wins[0].line[0], "Vsync: %lu", f);
        pad(wins[0].line[0], 24);
        sprintf(wins[0].line[1], "Time: %lu.%02lu", sec, frac);
        pad(wins[0].line[1], 24);
        sprintf(wins[0].line[2], "Sw:%lu", ss_switch_count);
        pad(wins[0].line[2], 24);

        ss_task_sleep(40);
    }
    return NULL;
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    int ssp = _iocs_b_super(0);

    ss_init_trap14();

    ss_mem_init(local_memory, sizeof(local_memory));
    ss_sched_init();

    main_tcb.state = SS_TS_READY;
    main_tcb.pri = 8;
    main_tcb.stack_base = (void*)1;
    ss_curr_task = &main_tcb;
    ss_sched_enqueue(&main_tcb);

    ss_set_interrupts();

    int old_mode = _iocs_crtmod(-1);
    _iocs_crtmod(8);
    _iocs_g_clr_on();
    set_palette();
    _iocs_b_curoff();
    _iocs_skeyset(0);

    ss_gfx_init();

    {
        volatile uint32_t* tp = (volatile uint32_t*)0xE00000;
        for (int i = 0; i < 0x1000; i++) tp[i] = 0;
    }

    _iocs_ms_init();
    _iocs_skey_mod(0, 0, 0);
    _iocs_ms_curon();
    _iocs_ms_limit(0, SS_SCREEN_W - 1, 0, SS_SCREEN_H - 1);

    saved_copy_vec = *(volatile uint32_t*)0xB0;
    saved_nmi_vec = *(volatile uint32_t*)0x7C;
    *(volatile uint32_t*)0xB0 = (uint32_t)ss_nop_handler;
    *(volatile uint32_t*)0x7C = (uint32_t)ss_nop_handler;

    wins[0] = (Win){30, 15, WIN_W, WIN_H, "Timer"};
    wins[1] = (Win){180, 60, WIN_W, WIN_H, "Keyboard"};
    wins[2] = (Win){80, 120, WIN_W, WIN_H, "Mouse"};
    ol_valid = 0;

    uint16_t t_data = ss_task_create(&(SSTaskInfo){.entry = data_thread,
                                                    .pri = 8,
                                                    .ctx_level = 0,
                                                    .stack_size = 0,
                                                    .stack = NULL});
    ss_task_start(t_data);

    for (;;) {
        wait_vsync();
        frame++;

        if (exit_flag) break;

        int cur_mx, cur_my, left;
        {
            int dt = _iocs_ms_getdt();
            int pos = _iocs_ms_curgt();
            cur_mx = (int16_t)((pos >> 16) & 0xFFFF);
            cur_my = (int16_t)(pos & 0xFFFF);
            left = (dt & 0x0200) != 0;
            mb_left = left;
            mb_right = (dt & 0x0001) != 0;
        }

        sprintf(wins[2].line[0], "X:%3d Y:%3d", cur_mx, cur_my);
        pad(wins[2].line[0], 24);
        sprintf(wins[2].line[1], "L=%s R=%s", left ? "DN" : "UP",
                mb_right ? "DN" : "UP");
        pad(wins[2].line[1], 24);

        if (left && drag < 0) {
            int hit = hit_test(cur_mx, cur_my);
            if (hit >= 0 && cur_my >= wins[hit].y + 1 &&
                cur_my <= wins[hit].y + TITLE_H) {
                drag = hit;
                dox = cur_mx - wins[hit].x;
                doy = cur_my - wins[hit].y;
                bring_to_front(hit);
                save_win_bitmap(wins[hit].x, wins[hit].y, wins[hit].w,
                                wins[hit].h);
                ss_gfx_fill_stipple(wins[hit].x, wins[hit].y,
                                    wins[hit].w, wins[hit].h,
                                    C_WHITE, C_GRAY_M);
                for (int i = 0; i < NUM_WINS; i++) {
                    int idx = zmap[i];
                    if (idx == drag) continue;
                    draw_frame(&wins[idx], (i == NUM_WINS - 1));
                    memset(wins[idx].prev, 0xFF, sizeof(wins[idx].prev));
                    draw_content_dirty(&wins[idx]);
                }
                ol_save(wins[drag].x, wins[drag].y, wins[drag].w, wins[drag].h);
                draw_march_outline(wins[drag].x, wins[drag].y, wins[drag].w,
                                   wins[drag].h);
                memset(wins[drag].prev, 0xFF, sizeof(wins[drag].prev));
                need_full = 0;
            }
        }
        if (!left && drag >= 0) {
            ol_restore();
            int wx = wins[drag].x, wy = wins[drag].y;
            int ww = wins[drag].w, wh = wins[drag].h;
            if (win_save_valid && wx >= 0 && wy >= 0 &&
                wx + ww <= SS_SCREEN_W && wy + wh <= SS_SCREEN_H) {
                restore_win_bitmap(wx, wy, ww, wh);
                ss_gfx_rect(wx + 1, wy + 1, ww - 2, TITLE_H - 2, C_GRAY_L);
                int tw = (int)strlen(wins[drag].title) * SS_FONT_ADV;
                int tx = wx + (ww - tw) / 2;
                for (int li = 0; li < 5; li++) {
                    int ly = wy + 2 + li * 2;
                    if (tx > wx + 12)
                        ss_gfx_rect(wx + 4, ly, tx - 8 - (wx + 4) + 1, 1, C_BLACK);
                    if (tx + tw + 8 < wx + ww - 4)
                        ss_gfx_rect(tx + tw + 8, ly, (wx + ww - 5) - (tx + tw + 8) + 1, 1, C_BLACK);
                }
                ss_gfx_draw_text(tx, wy + 2, wins[drag].title, C_BLACK, C_GRAY_L);
            } else {
                draw_frame(&wins[drag], 1);
            }
            memset(wins[drag].prev, 0xFF, sizeof(wins[drag].prev));
            draw_content_dirty(&wins[drag]);
            drag = -1;
        }
        if (drag >= 0) {
            wins[drag].x = cur_mx - dox;
            wins[drag].y = cur_my - doy;
        }

        if (need_full) {
            ss_gfx_fill_stipple(0, 0, SS_SCREEN_W, SS_SCREEN_H,
                                C_WHITE, C_GRAY_M);
            for (int i = 0; i < NUM_WINS; i++) {
                int idx = zmap[i];
                draw_frame(&wins[idx], (i == NUM_WINS - 1));
                memset(wins[idx].prev, 0xFF, sizeof(wins[idx].prev));
                draw_content_dirty(&wins[idx]);
            }
            need_full = 0;
        } else if (drag >= 0) {
            int moved = (ol_x != wins[drag].x || ol_y != wins[drag].y);
            if (moved) {
                ol_restore();
                ol_save(wins[drag].x, wins[drag].y, wins[drag].w, wins[drag].h);
            }
            draw_march_outline(wins[drag].x, wins[drag].y, wins[drag].w,
                               wins[drag].h);
        } else {
            for (int i = 0; i < NUM_WINS; i++) {
                int idx = zmap[i];
                Win* w = &wins[idx];
                for (int j = 0; j < 3; j++) {
                    if (memcmp(w->line[j], w->prev[j], 30) != 0) {
                        draw_text_clip(w->x + 4, w->y + CONTENT_Y + j * LINE_H,
                                       w->line[j], C_BLACK, C_WHITE, i);
                        memcpy(w->prev[j], w->line[j], 30);
                    }
                }
            }
        }
    }

    if (ss_trapbuf_flag != 0) {
        char buf[128];
        _iocs_b_print("\r\n=== EXCEPTION CAUGHT (TRAP #14) ===\r\n");
        sprintf(buf, "Type: %s (code=%d)\r\n", ss_trapbuf_msg, ss_trapbuf_flag);
        _iocs_b_print(buf);
        sprintf(buf, "PC: 0x%08X\r\n", ss_trapbuf_pc);
        _iocs_b_print(buf);
        sprintf(buf, "SR: 0x%04X\r\n", ss_trapbuf_sr);
        _iocs_b_print(buf);
        _iocs_b_print("====================================\r\n");
        _iocs_b_super(ssp);
        _exit(1);
    }

    ss_restore_interrupts();
    ss_restore_trap14();

    *(volatile uint32_t*)0xB0 = saved_copy_vec;
    *(volatile uint32_t*)0x7C = saved_nmi_vec;

    ol_restore();
    _iocs_ms_curof();
    _iocs_skey_mod(-1, 0, 0);
    _iocs_crtmod(old_mode);
    _iocs_b_curon();

    _iocs_b_print("SSOS-Preemptive terminated.\r\n");
    _iocs_b_super(ssp);
    _exit(0);
}

#endif /* LOCAL_MODE */
