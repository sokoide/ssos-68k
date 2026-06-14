#include "../kernel/kernel.h"
#include "../kernel/scheduler.h"
#include "../kernel/work_queue.h"
#include "../mem/memory.h"
#include "../gfx/gfx.h"
#include "../win/win.h"
#include "../ipc/ipc.h"
#include <stdint.h>

uint8_t* ss_task_stack_base;

/* Drag state */
static int drag_id = -1;
static int drag_ox, drag_oy;
static int drag_w, drag_h;
static int drag_prev_x = -1, drag_prev_y = -1;
static uint16_t next_z = 3;   /* monotonically increasing z for drag-to-front
                               * (starts above initial z=1,2; wraps 255→3 since
                               *  zmap is uint8_t and would truncate z>=256) */

/* VSYNC wait via MFP GPIP polling */
static void wait_vsync(void) {
    uint32_t last = ss_vsync_counter;
    while (ss_vsync_counter == last);
}

/*
 * Mouse input via IOCS _MS_CURGT ($75) + _MS_GETDT ($74).
 *
 * _MS_CURGT returns the absolute cursor position (X<<16 | Y) maintained
 * by the IPL-ROM SCC receive handler — the same source standalone/main.c
 * uses, so bare-metal and standalone track the cursor identically.
 * _MS_GETDT returns the button word: bit9 = left (00/FF), bit0 = right.
 *
 * These are IPL-ROM (BIOS) routines, not Human68K services: they work
 * bare-metal as long as interrupts stay enabled and the IOCS work area /
 * vector table are left intact (both guaranteed by premain).  Verified
 * working bare-metal: SCC handler installed (vector 0x148), IPL0, cursor
 * position and deltas both update live.  See x68k-master/10_キーボード・マウス.md.
 */
static int cur_mx = 0, cur_my = 0, cur_btn = 0;

static void update_mouse(void) {
    int pos, dt;
    asm volatile(
        "moveq #0x75, %%d0\n\t"   /* _MS_CURGT: absolute X<<16|Y */
        "trap #15\n\t"
        "move.l %%d0, %0\n\t"
        "moveq #0x74, %%d0\n\t"   /* _MS_GETDT: button word */
        "trap #15\n\t"
        "move.l %%d0, %1"
        : "=d"(pos), "=d"(dt) :: "d0"
    );
    cur_mx = (int16_t)((pos >> 16) & 0xFFFF);
    cur_my = (int16_t)(pos & 0xFFFF);
    cur_btn = dt;
}

/*
 * XOR rectangle outline — used for BOTH the mouse cursor (6x6) and the
 * drag outline (window-sized).  XOR is its own inverse: drawing the same
 * outline twice restores the exact original pixels.  Because XOR commutes,
 * overlapping outlines (cursor over drag-rect, or drag-rect over window)
 * compose and decompose correctly without any save buffer.  No full
 * repaint, no trail, no flicker.
 */
static int cur_prev_x = -100, cur_prev_y = -100;

static void xor_outline(int x, int y, int w, int h) {
    uint32_t stride = ss_current_mode->bytes_per_line / 2;
    int W = ss_current_mode->display_w, H = ss_current_mode->display_h;
    volatile uint16_t* v = ss_draw_page;
    /* top & bottom edges */
    for (int dx = 0; dx < w; dx++) {
        int xx = x + dx;
        if (xx >= 0 && xx < W) {
            if (y >= 0 && y < H) v[y * stride + xx] ^= 0xFFFF;
            int y2 = y + h - 1;
            if (y2 >= 0 && y2 < H) v[y2 * stride + xx] ^= 0xFFFF;
        }
    }
    /* left & right edges */
    for (int dy = 0; dy < h; dy++) {
        int yy = y + dy;
        if (yy >= 0 && yy < H) {
            if (x >= 0 && x < W) v[yy * stride + x] ^= 0xFFFF;
            int x2 = x + w - 1;
            if (x2 >= 0 && x2 < W) v[yy * stride + x2] ^= 0xFFFF;
        }
    }
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

void ss_run(void) {
    /* Create demo windows */
    uint16_t w1 = ss_win_create(50, 50, 300, 200, 1);
    uint16_t w2 = ss_win_create(200, 150, 300, 200, 2);
    (void)w1; (void)w2;

    /* Initial render */
    ss_win_render_all();

    /* Main loop */
    while (1) {
        wait_vsync();

        /* Mouse: CURGT absolute position (standalone-compatible) + buttons */
        update_mouse();
        int mx = cur_mx, my = cur_my, btn = cur_btn;
        int left = (btn & 0x0200) != 0;

        /*
         * Drag with outline only (no per-frame full repaint → no flicker),
         * mirroring standalone/main.c: while the button is held we move
         * just an XOR rectangle; the window body stays put until drop,
         * when we commit its final position and repaint once.
         */
        int need_redraw = 0;
        if (left && drag_id < 0) {
            int hid = ss_win_hit_test(mx, my);
            if (hid > 0) {
                drag_id = hid;
                drag_ox = mx - ss_win_get_x(hid);
                drag_oy = my - ss_win_get_y(hid);
                drag_w  = ss_win_get_w(hid);
                drag_h  = ss_win_get_h(hid);
                ss_win_set_z(hid, next_z);   /* unique z → front, no ties */
                if (next_z >= 255) next_z = 3;
                else next_z++;
                drag_prev_x = ss_win_get_x(hid);
                drag_prev_y = ss_win_get_y(hid);
                xor_outline(drag_prev_x, drag_prev_y, drag_w, drag_h);
            }
        } else if (left && drag_id > 0) {
            int nx = mx - drag_ox, ny = my - drag_oy;
            int W = ss_current_mode->display_w, H = ss_current_mode->display_h;
            if (nx < 0) nx = 0;
            if (ny < 0) ny = 0;
            if (nx + drag_w > W) nx = W - drag_w;
            if (ny + drag_h > H) ny = H - drag_h;
            if (nx != drag_prev_x || ny != drag_prev_y) {
                xor_outline(drag_prev_x, drag_prev_y, drag_w, drag_h); /* erase */
                xor_outline(nx, ny, drag_w, drag_h);                    /* draw */
                drag_prev_x = nx;
                drag_prev_y = ny;
            }
        } else if (!left && drag_id > 0) {
            xor_outline(drag_prev_x, drag_prev_y, drag_w, drag_h); /* erase */
            ss_win_move(drag_id, drag_prev_x, drag_prev_y);         /* commit */
            drag_id = -1;
            drag_prev_x = -1;
            need_redraw = 1;
        }

        /*
         * Repaint strategy: full repaint only on drop (window committed);
         * otherwise just erase the previous cursor outline via XOR.  The
         * drag outline (if active) is managed above and composes safely
         * with the cursor because both are XOR.
         */
        if (need_redraw) {
            ss_win_render_all();
            cur_prev_x = -100;   /* fresh background overwrote old cursor */
        } else if (cur_prev_x >= 0) {
            xor_outline(cur_prev_x, cur_prev_y, 6, 6);  /* erase prev cursor */
        }

        /* Software cursor: XOR outline at (mx,my).  Erased next frame by the
         * xor_outline(cur_prev_*) call above.  Composes safely with any
         * active drag outline because both are XOR. */
        xor_outline(mx, my, 6, 6);
        cur_prev_x = mx; cur_prev_y = my;

        ss_work_drain(&ss_main_work_queue);
        ss_process_wakeups();
        ss_task_yield();
    }
}
