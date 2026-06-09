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

/* VSYNC wait via MFP GPIP polling */
static void wait_vsync(void) {
    volatile uint8_t* gpip = (volatile uint8_t*)0xE88001;
    while (!(*gpip & 0x10));
    while (*gpip & 0x10);
}

/* Read mouse position via IOCS MS_CURGT (trap #15, D0=0x74) */
static int read_mouse_x(void) {
    int pos;
    asm volatile(
        "moveq #0x74, %%d0\n\t"
        "trap #15\n\t"
        "move.l %%d0, %0"
        : "=d"(pos) : : "d0"
    );
    return (int16_t)((uint32_t)pos >> 16);
}
static int read_mouse_y(void) {
    int pos;
    asm volatile(
        "moveq #0x74, %%d0\n\t"
        "trap #15\n\t"
        "move.l %%d0, %0"
        : "=d"(pos) : : "d0"
    );
    return (int16_t)(pos & 0xFFFF);
}
static int read_mouse_buttons(void) {
    int dt;
    asm volatile(
        "moveq #0x73, %%d0\n\t"
        "trap #15\n\t"
        "move.l %%d0, %0"
        : "=d"(dt) : : "d0"
    );
    return dt;
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

        int mx = read_mouse_x();
        int my = read_mouse_y();
        int btn = read_mouse_buttons();
        int left = (btn & 0x0200) != 0;  /* left button pressed */

        /* Left button released: stop drag */
        if (!left && drag_id >= 0) {
            drag_id = -1;
        }

        /* Redraw only when needed (not every frame like standalone) */
        int need_redraw = 0;
        if (left) {
            if (drag_id < 0) {
                int hid = ss_win_hit_test(mx, my);
                if (hid > 0) {
                    drag_id = hid;
                    drag_ox = mx - ss_win_get_x(hid);
                    drag_oy = my - ss_win_get_y(hid);
                    ss_win_set_z(hid, 255);
                    ss_win_mark_dirty(hid);
                    need_redraw = 1;
                }
            } else {
                int new_x = mx - drag_ox;
                int new_y = my - drag_oy;
                if (new_x != ss_win_get_x(drag_id) ||
                    new_y != ss_win_get_y(drag_id)) {
                    ss_win_move(drag_id, new_x, new_y);
                    need_redraw = 1;
                }
            }
        }
        if (need_redraw) {
            ss_win_render_all();
        }

        ss_work_drain(&ss_main_work_queue);
        ss_process_wakeups();
    }
}
