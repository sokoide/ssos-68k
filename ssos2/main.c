/*
 * SSOS2 — Multi-Threaded Windowed GUI for X68000
 *
 * Features:
 *   - Cooperative multitasking (3 tasks, round-robin)
 *   - Multiple draggable windows
 *   - Back-buffer rendering (fast, no VRAM contention)
 *   - Timer D interrupt for 1ms tick counter
 *   - V-sync synchronized display updates
 *   - Software mouse cursor
 *
 * Tasks:
 *   Task 0 (Main):  Input handling, rendering, VRAM transfer
 *   Task 1 (Counter): Updates a counting window every second
 *   Task 2 (Clock):  Updates a clock window with ms timer
 *
 * Build:  source ~/.elf2x68k && make
 * Run:    Copy ~/tmp/ssos2.x to XEiJ emulator
 * Exit:   Press ESC
 */

#include "gui.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <x68k/iocs.h>

/* ===========================================================
 * Window definitions (registered in main)
 * =========================================================== */

/* Window indices (set during setup) */
static int win_counter = -1;
static int win_clock = -1;

/* ===========================================================
 * Timer setup
 * =========================================================== */

static uint32_t saved_timerd_vec;

void timer_init(void) {
    volatile uint8_t* mfp = MFP_BASE;

    /* Save old Timer D interrupt vector (at address 0x110) */
    saved_timerd_vec = *(volatile uint32_t*)0x110;

    /* Install our handler */
    *(volatile uint32_t*)0x110 = (uint32_t)timerd_isr;

    /* Configure Timer D: 4MHz / 50 / 80 = 1000Hz (1ms) */
    mfp[0x25] = 80;                        /* TDDR: count 80  */
    mfp[0x1d] = (mfp[0x1d] & 0xF0) | 0x04; /* TCDCR: Timer D = /50 */

    /* Enable Timer D interrupt in MFP */
    mfp[0x09] |= 0x10; /* IERB: enable Timer D (bit 4) */
    mfp[0x15] |= 0x10; /* IMRB: unmask Timer D (bit 4) */
}

void timer_cleanup(void) {
    volatile uint8_t* mfp = MFP_BASE;

    /* Disable Timer D */
    mfp[0x09] &= ~0x10;
    mfp[0x15] &= ~0x10;

    /* Restore original vector */
    *(volatile uint32_t*)0x110 = saved_timerd_vec;
}

/* ===========================================================
 * Background Task 1: Counter window
 * =========================================================== */

void task_counter(void) {
    uint32_t count = 0;
    uint32_t last_tick = ssos2_tick;

    for (;;) {
        /* Update once per second */
        if (ssos2_tick - last_tick >= 1000) {
            last_tick += 1000;
            count++;

            if (win_counter >= 0) {
                sprintf(g_wins[win_counter].line1, "Count: %lu", count);
                sprintf(g_wins[win_counter].line2, "Running...");
                g_wins[win_counter].dirty = 1;
            }
        }

        task_yield();
    }
}

/* ===========================================================
 * Background Task 2: Clock window (updates ~60fps)
 * =========================================================== */

void task_clock(void) {
    uint32_t last_clock = 0;
    for (;;) {
        /* Update clock every 100ms instead of every yield */
        if (ssos2_tick - last_clock >= 100) {
            last_clock = ssos2_tick;
            if (win_clock >= 0) {
                uint32_t t = ssos2_tick;
                uint32_t sec = t / 1000;
                uint32_t min = sec / 60;
                uint32_t hrs = min / 60;
                sprintf(g_wins[win_clock].line1, "%02lu:%02lu:%02lu", hrs % 24,
                        min % 60, sec % 60);
                sprintf(g_wins[win_clock].line2, "%lu ms", t);
                g_wins[win_clock].dirty = 1;
            }
        }

        task_yield();
    }
}

/* ===========================================================
 * Helper: clamp value to range
 * =========================================================== */
static int clamp(int v, int lo, int hi) {
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/* ===========================================================
 * Main task (Task 0): Input + Rendering + VRAM transfer
 * =========================================================== */

int main(void) {
    /* Enter supervisor mode */
    int ssp = _iocs_b_super(0);

    /* --- Initialize subsystems --- */
    gui_init();
    timer_init();
    _iocs_ms_init();
    _iocs_skey_mod(0, 0, 0); /* enable mouse button handling */
    _iocs_ms_curon(); /* activate IOCS mouse tracking (required for _MS_GETDT)
                       */

    /* --- Create windows --- */
    Window w1 = {
        .x = 80,
        .y = 60,
        .w = 280,
        .h = 120,
        .border_color = COL_WHITE,
        .title_bg = COL_BLUE,
        .title_fg = COL_WHITE,
        .body_bg = COL_LGRAY,
        .title = "Counter",
    };
    win_counter = gui_register_window(&w1);

    Window w2 = {
        .x = 400,
        .y = 200,
        .w = 280,
        .h = 120,
        .border_color = COL_WHITE,
        .title_bg = COL_PURPLE,
        .title_fg = COL_WHITE,
        .body_bg = COL_LGRAY,
        .title = "Clock",
    };
    win_clock = gui_register_window(&w2);

    /* --- Initialize multitasking --- */
    task_init();
    task_create(task_counter);
    task_create(task_clock);

    /* --- Mouse state --- */
    int mouse_x = SCREEN_W / 2;
    int mouse_y = SCREEN_H / 2;
    int mouse_x_prev = mouse_x;
    int mouse_y_prev = mouse_y;
    int dragging = 0;
    int drag_win = -1;
    int drag_off_x = 0, drag_off_y = 0;
    int drag_frame_x = 0, drag_frame_y = 0;
    int drag_old_frame_x = -1, drag_old_frame_y = -1;
    uint32_t dt = 0;
    int running = 1;

    /* --- Initial draw + full VRAM transfer --- */
    gui_draw_all_windows();
    /* Don't bake cursor in full flip; XOR loop handles it */
    gui_wait_vsync();
    gui_flip_full();

    int first_frame = 1;

    /* ===========================================================
     * Main loop
     * =========================================================== */
    while (running) {
        int needs_vram_flip = first_frame;
        int needs_backbuf_refresh = 0;

        /* --- Check ESC key --- */
        if (_iocs_b_keysns() > 0) {
            int key = _iocs_b_keyinp();
            if ((key & 0xFF) == 0x1B) {
                running = 0;
                break;
            }
        }

        /* --- Accumulate dirty regions for this frame --- */
        dirty_reset();

        /* Ensure cursor appears on first frame */
        if (first_frame) {
            dirty_include_cursor(mouse_x, mouse_y);
            first_frame = 0;
        }

        /* --- Read mouse --- */
        {
            dt = _iocs_ms_getdt();
            uint32_t pos = _iocs_ms_curgt();

            mouse_x_prev = mouse_x;
            mouse_y_prev = mouse_y;
            mouse_x = clamp((int)((pos >> 16) & 0xFFFF), 0, SCREEN_W - 1);
            mouse_y = clamp((int)(pos & 0xFFFF), 0, SCREEN_H - 1);

            int left_pressed = ((dt & 0xFF00) >> 8) != 0;

            if (left_pressed && !dragging) {
                int hit = gui_hit_test_window(mouse_x, mouse_y);
                if (hit >= 0) {
                    dragging = 1;
                    drag_win = hit;
                    drag_off_x = mouse_x - g_wins[hit].win.x;
                    drag_off_y = mouse_y - g_wins[hit].win.y;
                    drag_frame_x = g_wins[hit].win.x;
                    drag_frame_y = g_wins[hit].win.y;
                    drag_old_frame_x = drag_frame_x;
                    drag_old_frame_y = drag_frame_y;
                    
                    /* Bring window to top and bake the background */
                    gui_bring_to_top(hit);
                    gui_draw_all_windows();
                    
                    /* Mark full screen for flip once to establish stable bg */
                    needs_vram_flip = 1;
                    dirty_include(0, 0, SCREEN_W, SCREEN_H);
                }
            }

            if (dragging) {
                if (!left_pressed) {
                    /* Finalize drag: update actual position and bake into backbuf */
                    Window* w = &g_wins[drag_win].win;
                    
                    /* Dirty the old AND the new finalized positions for VRAM update */
                    dirty_include_window(w); /* old stationary content pos */
                    w->x = drag_frame_x;
                    w->y = drag_frame_y;
                    dirty_include_window(w); /* new finalized content pos */
                    
                    /* Also dirty the frame's last position to clear it from VRAM */
                    dirty_include(drag_frame_x, drag_frame_y, w->w, w->h);

                    dragging = 0;
                    drag_win = -1;
                    
                    /* Redraw all windows to bake the new state */
                    gui_draw_all_windows();
                    needs_vram_flip = 1;
                } else {
                    /* Pure frame movement */
                    drag_old_frame_x = drag_frame_x;
                    drag_old_frame_y = drag_frame_y;
                    Window* w = &g_wins[drag_win].win;
                    drag_frame_x = clamp(mouse_x - drag_off_x, 0, SCREEN_W - w->w);
                    drag_frame_y = clamp(mouse_y - drag_off_y, 0, SCREEN_H - w->h);
                    
                    /* Include old and new frame positions in dirty region for XOR flip */
                    dirty_include(drag_old_frame_x, drag_old_frame_y, w->w, w->h);
                    dirty_include(drag_frame_x, drag_frame_y, w->w, w->h);
                    
                    /* We MUST animate the dotted line frame even if stationary */
                    needs_vram_flip = 1;
                }
            }

            if (mouse_x != mouse_x_prev || mouse_y != mouse_y_prev) {
                dirty_include_cursor(mouse_x_prev, mouse_y_prev);
                dirty_include_cursor(mouse_x, mouse_y);
                needs_vram_flip = 1;
            }
        }

        /* --- Let background tasks run --- */
        task_yield();

        /* --- Update window contents (ONLY when NOT dragging) --- */
        if (!dragging) {
            if (win_counter >= 0) {
                sprintf(g_wins[win_counter].line2, "dt:%08lX %d,%d", dt, mouse_x,
                        mouse_y);
                g_wins[win_counter].dirty = 1;
            }

            for (int i = 0; i < g_num_wins; i++) {
                if (g_wins[i].dirty) {
                    dirty_include_window(&g_wins[i].win);
                    needs_backbuf_refresh = 1;
                    needs_vram_flip = 1;
                }
            }
        }

        /* --- Fast Redraw & Flip --- */
        if (needs_vram_flip) {
            if (needs_backbuf_refresh) {
                /* Only redraw stationary content if we aren't dragging */
                gui_draw_app_windows();
            }

            /* Capture phase once to ensure XOR restoration is perfect */
            int phase = ssos2_tick / 2;

            /* 1. Temporarily apply XOR elements to backbuf */
            if (dragging && drag_win >= 0) {
                Window* w = &g_wins[drag_win].win;
                gui_draw_dotted_rect_xor(drag_frame_x, drag_frame_y, w->w, w->h,
                                         phase);
            }
            gui_draw_cursor_xor(mouse_x, mouse_y);

            /* 2. Transfer dirty regions to VRAM */
            gui_wait_vsync();
            gui_flip_dirty_region();

            /* 3. Restore backbuf by XORing again (reverses step 1) */
            gui_draw_cursor_xor(mouse_x, mouse_y);
            if (dragging && drag_win >= 0) {
                Window* w = &g_wins[drag_win].win;
                gui_draw_dotted_rect_xor(drag_frame_x, drag_frame_y, w->w, w->h,
                                         phase);
            }
        } else {
            gui_wait_vsync();
        }
    }

    /* --- Cleanup --- */
    timer_cleanup();
    _iocs_ms_curof();         /* deactivate mouse cursor */
    _iocs_skey_mod(-1, 0, 0); /* restore shift key mode */
    _iocs_g_clr_on();
    _iocs_b_curon();
    _iocs_crtmod(16);

    /* Leave supervisor mode */
    _iocs_b_super(ssp);

    return 0;
}
