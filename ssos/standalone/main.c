/*
 * SSOS Standalone — multithreaded Windowed Demo
 * CRT: 512x512 256-color (crtmod 8) or 1024x1024 16-color (crtmod 16)
 * Font: Spleen 5x8
 * Exit: ESC key
 *
 * Architecture:
 *   Thread 1 (main, pri=8):   Rendering loop + mouse IOCS (V-sync rate)
 *   Thread 2 (data, pri=8):   Timer/Keyboard window data + IOCS calls
 *   Context switch: cooperative = explicit yield (SCHED=cooperative)
 *                   preemptive  = Timer D ISR     (SCHED=preemptive)
 */
#include "../os/kernel/kernel.h"
#include "../os/kernel/scheduler.h"
#include "../os/mem/memory.h"
#include "../os/gfx/gfx.h"
#include "../os/win/win.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../os/util/numfmt.h"

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

/* Diagnostic cases for s33 crash investigation.
 * Define one of these via -D on the command line:
 *   make CDEFINES=-DDIAG_CASE_1   (etc.)
 * Runs the test in place of the normal exit sequence. */
#ifdef DIAG_CASE_1
/* B_SUPER → dos_exit2(0) — no B_PRINT at all */
#include <x68k/dos.h>
#endif
#ifdef DIAG_CASE_2
/* B_SUPER → 1000 nops → dos_exit2(0) */
#include <x68k/dos.h>
#endif
#ifdef DIAG_CASE_3
/* B_SUPER → _iocs_b_keysns() → dos_exit2(0) */
#include <x68k/dos.h>
#endif
#ifdef DIAG_CASE_4
/* B_SUPER → B_PRINT("A") → dos_exit2(0) */
#include <x68k/dos.h>
#endif
#ifdef DIAG_CASE_5
/* B_SUPER → B_PRINT("AAAA...") → dos_exit2(0) */
#include <x68k/dos.h>
#endif

#ifdef LOCAL_MODE

static uint8_t local_memory[512 * 1024] __attribute__((aligned(4)));
static uint8_t local_stack_mem[SS_MAX_TASKS * SS_TASK_STACK]
    __attribute__((aligned(4)));

uint8_t* ss_task_stack_base = local_stack_mem;

extern uint32_t ss_context_switch_count;

/* Palette Indices - Runtime mode-dependent */
static int c_white, c_black, c_gray_l, c_gray_m, c_gray_d;

static void init_palette_indices(void) {
    if (ss_current_mode->color_count == 16) {
        /* 16-color mode palette indices */
        c_white = 7;       /* White */
        c_black = 0;       /* Black */
        c_gray_l = 8;      /* Light Gray */
        c_gray_m = 15;     /* Dark Gray (background) */
        c_gray_d = 0;      /* Black */
    } else {
        /* 256-color mode palette indices */
        c_white = 0;
        c_black = 215;
        c_gray_l = 247;
        c_gray_m = 250;
        c_gray_d = 252;
    }
}

#define C_WHITE c_white
#define C_BLACK c_black
#define C_GRAY_L c_gray_l
#define C_GRAY_M c_gray_m
#define C_GRAY_D c_gray_d

#define PAL_RGB(r, g, b)                                                       \
    (uint16_t)((((g) & 0x1F) << 11) | (((r) & 0x1F) << 6) |                    \
               (((b) & 0x1F) << 1) | 1)

static void set_palette(void) {
    if (ss_current_mode->color_count == 256) {
        /* 256-color palette (6x6x6 cube + 40 system colors) */
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
    } else if (ss_current_mode->color_count == 16) {
        /* 16-color palette (basic colors) */
        static const uint16_t palette_16[16] = {
            PAL_RGB(0, 0, 0),       /* 0: Black */
            PAL_RGB(0, 0, 31),      /* 1: Blue */
            PAL_RGB(0, 31, 0),      /* 2: Green */
            PAL_RGB(0, 31, 31),     /* 3: Cyan */
            PAL_RGB(31, 0, 0),      /* 4: Red */
            PAL_RGB(31, 0, 31),     /* 5: Magenta */
            PAL_RGB(31, 31, 0),     /* 6: Yellow */
            PAL_RGB(31, 31, 31),    /* 7: White */
            PAL_RGB(15, 15, 15),    /* 8: Dark Gray */
            PAL_RGB(15, 15, 31),    /* 9: Light Blue */
            PAL_RGB(15, 31, 15),    /* 10: Light Green */
            PAL_RGB(15, 31, 31),    /* 11: Light Cyan */
            PAL_RGB(31, 15, 15),    /* 12: Light Red */
            PAL_RGB(31, 15, 31),    /* 13: Light Magenta */
            PAL_RGB(31, 31, 15),    /* 14: Light Yellow */
            PAL_RGB(0, 0, 0),       /* 15: Black (duplicate) */
        };
        for (int i = 0; i < 16; i++) {
            _iocs_gpalet(i, palette_16[i]);
        }
    }
}

/* Window layout */
#define TITLE_H 12
#define CONTENT_Y 14
#define LINE_H 10
#define LINE_LEN 28   /* content lines are space-padded to this width so
                       * draw_content_dirty can redraw only the changed
                       * suffix without leaving stale trailing chars */
#define WIN_W 240
#define WIN_H (CONTENT_Y + 3 * LINE_H + 4)

/* Space-pad `s` to exactly LINE_LEN chars (NUL at [LINE_LEN]). Mirrors the
 * app/ version; required for the differential content redraw below. */
static void pad_line(char* s, int n) {
    int l = (int)strlen(s);
    for (int i = l; i < n; i++) s[i] = ' ';
    s[n] = '\0';
}

static uint16_t next_z = 4;
static uint16_t win_ids[3];
static volatile uint32_t frame = 0;

static int id_to_idx(uint16_t id) {
    for (int i = 0; i < 3; i++)
        if (win_ids[i] == id) return i;
    return -1;
}
static volatile int last_key = -1;
static volatile int mx, my;  /* Initialized in main() based on current mode */
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

extern volatile uint8_t ss_wakeups_needed;
extern void ss_process_wakeups(void);

static SSTask main_tcb;

/* Drag outline position tracker (self-erasing XOR rect via ss_gfx_xor_rect).
 * No save buffer: the outline is erased by re-XORing the same rect. */
static int ol_x, ol_y;

/* ---- GVRAM bitmap save/restore (direct page 0 access) ---- */

static void save_win_bitmap(int x, int y, int w, int h) {
    uint32_t stride = ss_current_mode->bytes_per_line / 2;  /* words per line */
    int disp_w = ss_current_mode->display_w;
    int disp_h = ss_current_mode->display_h;

    /* Clip to screen bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > disp_w) w = disp_w - x;
    if (y + h > disp_h) h = disp_h - y;
    if (w <= 0 || h <= 0) return;

    volatile uint16_t* s = ss_draw_page + (uint32_t)y * stride + x;
    for (int row = 0; row < h; row++) {
        for (int i = 0; i < w; i++) win_save_buf[row * w + i] = s[i];
        s += stride;
    }
    win_save_valid = 1;
}

static void restore_win_bitmap(int x, int y, int w, int h) {
    uint32_t stride = ss_current_mode->bytes_per_line / 2;  /* words per line */
    int disp_w = ss_current_mode->display_w;
    int disp_h = ss_current_mode->display_h;

    /* Clip to screen bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > disp_w) w = disp_w - x;
    if (y + h > disp_h) h = disp_h - y;
    if (w <= 0 || h <= 0) return;

    volatile uint16_t* d = ss_draw_page + (uint32_t)y * stride + x;
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
        d += stride;
    }
    win_save_valid = 0;
}

/* ---- Window frame drawing ---- */

static void draw_frame(SSWindow* w, int is_fg) {
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

    ss_gfx_draw_text_fast(tx, w->y + 2, w->title, C_BLACK, t_bg);
}

/* ---- Dirty content update ---- */

static void draw_content_dirty(SSWindow* w) {
    for (int i = 0; i < 3; i++) {
        if (memcmp(w->content[i], w->content_prev[i], 30) != 0) {
            /* Redraw only the changed suffix. Content is space-padded by
             * ss_win_set_content_line, so the first differing column marks
             * the visible change and the padded tail erases any shrink. */
            int j = 0;
            while (j < LINE_LEN && w->content[i][j] == w->content_prev[i][j]) j++;
            ss_gfx_draw_text_fast(w->x + 4 + j * SS_FONT_ADV,
                             w->y + CONTENT_Y + i * LINE_H,
                             w->content[i] + j, C_BLACK, C_WHITE);
            memcpy(w->content_prev[i], w->content[i], 30);
        }
    }
}

/* ---- Marching ants outline ---- */

/* The drag outline is now the shared self-erasing ss_gfx_xor_rect — no
 * per-frame ol_save (GVRAM read) / ol_restore / marching-ants redraw.
 * See drag_begin / drag_move / drag_end in the main loop. */

/* ---- V-sync with watchdog ---- */

static void wait_vsync(void) {
    uint32_t last = ss_vsync_counter;
    uint32_t last_vdisp = ss_vdisp_fire_count;
    uint32_t spin = 0;

    while (ss_vsync_counter == last) {
        spin++;
        /* Watchdog: ~55M spins at 10MHz ≈ 5s timeout.
         * If V-DISP ISR stops, break and try to recover. */
        if (spin > 5000000) {
            uint32_t vdisp_now = ss_vdisp_fire_count;
            uint32_t tick_now = ss_timerd_fire_count;
            _iocs_b_print("\r\n[WD] V-DISP stopped! "
                          "vdisp_fire=");
            /* Simple hex print via IOCS */
            {
                char buf[64];
                snprintf(buf, sizeof(buf),
                         "%lu tick_fire=%lu IMRA=%02X TACR=%02X "
                         "TCDCR=%02X IMRB=%02X ISRB=%02X\r\n",
                         vdisp_now, tick_now,
                         (unsigned)SS_MFP_IMRA, (unsigned)SS_MFP_TACR,
                         (unsigned)SS_MFP_TCDCR, (unsigned)SS_MFP_IMRB,
                         (unsigned)SS_MFP_ISRB);
                _iocs_b_print(buf);
            }
            _iocs_b_print("[WD] Attempting MFP re-init...\r\n");
            /* Re-enable V-DISP and Timer D */
            SS_MFP_IMRA = 0x21;
            SS_MFP_TACR = 0x08;
            SS_MFP_IMRB = 0x10;
            /* Reinstall V-DISP handler (vector may be corrupted) */
            extern void ss_vdisp_handler(void);
            *(volatile uint32_t*)0x134 = (uint32_t)ss_vdisp_handler;
            extern void ss_timerd_handler(void);
            *(volatile uint32_t*)0x110 = (uint32_t)ss_timerd_handler;
            /* Reset pending */
            SS_MFP_ISRA = 0x00;
            SS_MFP_ISRB = 0x00;
            spin = 0;
            last = ss_vsync_counter;
        }
    }
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
            char buf[30];
            /* "Code:XXH 'c'" built without sprintf */
            memcpy(buf, "Code:", 5);
            ss_utoa_hex((uint32_t)k, buf + 5, 2);
            buf[7] = 'H'; buf[8] = ' '; buf[9] = '\''; buf[10] = ch; buf[11] = '\''; buf[12] = '\0';
            ss_win_set_content_line(win_ids[1], 0, buf);
            /* "Shift:XXH" */
            memcpy(buf, "Shift:", 6);
            ss_utoa_hex((uint32_t)((last_key >> 8) & 0xFF), buf + 6, 2);
            buf[8] = 'H'; buf[9] = '\0';
            ss_win_set_content_line(win_ids[1], 1, buf);
        } else {
            ss_win_set_content_line(win_ids[1], 0, "Press any key...");
            ss_win_set_content_line(win_ids[1], 1, "");
        }

        char buf[30];
        /* "Vsync: <n>" */
        memcpy(buf, "Vsync: ", 7);
        ss_utoa_dec(frame, buf + 7);
        ss_win_set_content_line(win_ids[0], 0, buf);
        /* "VDisp:<n> Tick:<n>" */
        memcpy(buf, "VDisp:", 6);
        int nn = ss_utoa_dec(ss_vdisp_fire_count, buf + 6);
        memcpy(buf + 6 + nn, " Tick:", 6);
        ss_utoa_dec(ss_timerd_fire_count, buf + 6 + nn + 6);
        ss_win_set_content_line(win_ids[0], 1, buf);
        /* "Tick:<n>" */
        memcpy(buf, "Tick:", 5);
        ss_utoa_dec(ss_tick_counter, buf + 5);
        ss_win_set_content_line(win_ids[0], 2, buf);

        ss_task_sleep(40);
    }
    return NULL;
}

/* ================================================================
 * Main
 * ================================================================ */

int main(int argc, char** argv) {
    /* Parse command line arguments for graphics mode */
    int requested_mode = SS_CRTMOD_16;  /* Default: mode 16 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-8") == 0) {
            requested_mode = SS_CRTMOD_8;
        } else if (strcmp(argv[i], "-16") == 0) {
            requested_mode = SS_CRTMOD_16;
        }
    }
    ss_gfx_set_mode(requested_mode);
    init_palette_indices();

    /* Initialize mouse position to screen center */
    mx = ss_current_mode->display_w / 2;
    my = ss_current_mode->display_h / 2;

    /*
     * Enter Supervisor mode via raw IOCS _B_SUPER trap.
     * Can NOT use _iocs_b_super(1) — the library function executes
     * move.l %sp,%usp (privileged) BEFORE the trap, which crashes
     * from User mode.  A raw trap #15 bypasses this issue.
     */
    int old_ssp;
    asm volatile(
        "moveq #-127, %%d0\n\t"  /* _B_SUPER = 0x81 */
        "moveq #1, %%d1\n\t"     /* 1 = enter supervisor */
        "trap #15\n\t"
        "move.l %%d0, %0"
        : "=d"(old_ssp)
        :
        : "d0", "d1"
    );

    ss_init_trap14();

    ss_mem_init(local_memory, sizeof(local_memory));
    ss_sched_init();

    main_tcb.state = SS_TS_READY;
    main_tcb.pri = 8;
    main_tcb.stack_base = (void*)1;
    ss_curr_task = &main_tcb;
    ss_sched_enqueue(&main_tcb);

    int old_mode = _iocs_crtmod(-1);
    _iocs_crtmod(ss_current_mode->crtmod);
    _iocs_g_clr_on();
    set_palette();
    _iocs_b_curoff();
    _iocs_skeyset(0);

    ss_gfx_init();
    ss_win_init();

    {
        volatile uint32_t* tp = (volatile uint32_t*)0xE00000;
        for (int i = 0; i < 0x1000; i++) tp[i] = 0;
    }

    _iocs_ms_init();
    _iocs_skey_mod(0, 0, 0);
    _iocs_ms_curon();
    _iocs_ms_limit(0, ss_current_mode->display_w - 1, 0, ss_current_mode->display_h - 1);

    /*
     * Set up MFP interrupts LAST — IOCS calls above (crtmod, ms_init)
     * reprogram the MFP and would overwrite our settings if we called
     * ss_set_interrupts() before them.
     */
    ss_set_interrupts();

    saved_copy_vec = *(volatile uint32_t*)0xB0;
    saved_nmi_vec = *(volatile uint32_t*)0x7C;
    *(volatile uint32_t*)0xB0 = (uint32_t)ss_nop_handler;
    *(volatile uint32_t*)0x7C = (uint32_t)ss_nop_handler;

    win_ids[0] = ss_win_create(30, 15, WIN_W, WIN_H, 1);
    win_ids[1] = ss_win_create(180, 60, WIN_W, WIN_H, 2);
    win_ids[2] = ss_win_create(80, 120, WIN_W, WIN_H, 3);
    ss_win_set_title(win_ids[0], "Timer");
    ss_win_set_title(win_ids[1], "Keyboard");
    ss_win_set_title(win_ids[2], "Mouse");

    uint16_t t_data = ss_task_create(&(SSTaskInfo){.entry = data_thread,
                                                     .pri = 8,
                                                     .ctx_level = 0,
                                                     .stack_size = 0,
                                                      .stack = NULL});
    ss_task_start(t_data);

    int highest_active_z = -1;

    for (;;) {
        /* Process timer-based wakeups before anything else */
        ss_process_wakeups();

        wait_vsync();
        frame++;

        if (exit_flag) break;

        /* Recalculate highest_active_z every frame to handle z-order wraparound */
        highest_active_z = -1;
        for (int i = 0; i < 3; i++) {
            int z = ss_win_get_z(win_ids[i]);
            if (z > highest_active_z) highest_active_z = z;
        }

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

        char mbuf[30];
        /* "X:%3d Y:%3d" built without sprintf */
        mbuf[0] = 'X'; mbuf[1] = ':';
        int mn = ss_itoa_dec_pad(cur_mx, mbuf + 2, 3);
        int moff = 2 + mn;
        mbuf[moff] = ' '; mbuf[moff + 1] = 'Y'; mbuf[moff + 2] = ':';
        int mn2 = ss_itoa_dec_pad(cur_my, mbuf + moff + 3, 3);
        mbuf[moff + 3 + mn2] = '\0';
        ss_win_set_content_line(win_ids[2], 0, mbuf);
        /* "L=DN/UP R=DN/UP" */
        mbuf[0] = 'L'; mbuf[1] = '=';
        mbuf[2] = left ? 'D' : 'U'; mbuf[3] = left ? 'N' : 'P';
        mbuf[4] = ' '; mbuf[5] = 'R'; mbuf[6] = '=';
        mbuf[7] = mb_right ? 'D' : 'U'; mbuf[8] = mb_right ? 'N' : 'P';
        mbuf[9] = '\0';
        ss_win_set_content_line(win_ids[2], 1, mbuf);

        if (left && drag < 0) {
            int hid = ss_win_hit_test(cur_mx, cur_my);
            if (hid >= 0 && cur_my >= ss_win_get_y(hid) + 1 &&
                cur_my <= ss_win_get_y(hid) + TITLE_H) {
                drag = id_to_idx(hid);
                dox = cur_mx - ss_win_get_x(hid);
                doy = cur_my - ss_win_get_y(hid);
                ss_win_set_z(hid, next_z);
                if (++next_z > 255) next_z = 4;
                save_win_bitmap(ss_win_get_x(hid), ss_win_get_y(hid),
                                ss_win_get_w(hid), ss_win_get_h(hid));
                ss_gfx_fill_stipple(ss_win_get_x(hid), ss_win_get_y(hid),
                                    ss_win_get_w(hid), ss_win_get_h(hid),
                                    C_WHITE, C_GRAY_M);

                /* Recalculate highest_active_z after z-order change */
                highest_active_z = -1;
                for (int i = 0; i < 3; i++) {
                    int z = ss_win_get_z(win_ids[i]);
                    if (z > highest_active_z) highest_active_z = z;
                }

                for (int i = 0; i < 3; i++) {
                    uint16_t id = win_ids[i];
                    if (id == hid) continue;
                    SSWindow* w = ss_win_get_ptr(id);
                    draw_frame(w, ss_win_get_z(id) == highest_active_z);
                    memset(w->content_prev, 0xFF, sizeof(w->content_prev));
                    draw_content_dirty(w);
                }
                SSWindow* dw = ss_win_get_ptr(hid);
                /* Self-erasing XOR outline at the grab position. */
                ss_gfx_xor_rect(ss_win_get_x(hid), ss_win_get_y(hid),
                                ss_win_get_w(hid), ss_win_get_h(hid));
                ol_x = ss_win_get_x(hid);
                ol_y = ss_win_get_y(hid);
                memset(dw->content_prev, 0xFF, sizeof(dw->content_prev));
                need_full = 0;
            }
        }
        if (!left && drag >= 0) {
            uint16_t did = win_ids[drag];
            int wx = ss_win_get_x(did), wy = ss_win_get_y(did);
            int ww = ss_win_get_w(did), wh = ss_win_get_h(did);
            SSWindow* dw = ss_win_get_ptr(did);
            /* Erase the XOR outline at its last position. */
            ss_gfx_xor_rect(ol_x, ol_y, ww, wh);
            if (win_save_valid && wx >= 0 && wy >= 0 &&
                wx + ww <= ss_current_mode->display_w && wy + wh <= ss_current_mode->display_h) {
                restore_win_bitmap(wx, wy, ww, wh);
                ss_gfx_rect(wx + 1, wy + 1, ww - 2, TITLE_H - 2, C_GRAY_L);
                int tw = (int)strlen(dw->title) * SS_FONT_ADV;
                int tx = wx + (ww - tw) / 2;
                for (int li = 0; li < 5; li++) {
                    int ly = wy + 2 + li * 2;
                    if (tx > wx + 12)
                        ss_gfx_rect(wx + 4, ly, tx - 8 - (wx + 4) + 1, 1, C_BLACK);
                    if (tx + tw + 8 < wx + ww - 4)
                        ss_gfx_rect(tx + tw + 8, ly, (wx + ww - 5) - (tx + tw + 8) + 1, 1, C_BLACK);
                }
                ss_gfx_draw_text_fast(tx, wy + 2, dw->title, C_BLACK, C_GRAY_L);
            } else {
                draw_frame(dw, 1);
            }
            memset(dw->content_prev, 0xFF, sizeof(dw->content_prev));
            draw_content_dirty(dw);
            drag = -1;
        }
        if (drag >= 0) {
            uint16_t did = win_ids[drag];
            int nx = cur_mx - dox;
            int ny = cur_my - doy;
            /* Clamp window position to keep it on screen */
            if (nx < 0) nx = 0;
            if (ny < 0) ny = 0;
            if (nx + ss_win_get_w(did) > ss_current_mode->display_w)
                nx = ss_current_mode->display_w - ss_win_get_w(did);
            if (ny + ss_win_get_h(did) > ss_current_mode->display_h)
                ny = ss_current_mode->display_h - ss_win_get_h(did);
            ss_win_move(did, nx, ny);
        }

        if (need_full) {
            ss_gfx_fill_stipple(0, 0, ss_current_mode->display_w, ss_current_mode->display_h,
                                C_WHITE, C_GRAY_M);
            for (int i = 0; i < 3; i++) {
                SSWindow* w = ss_win_get_ptr(win_ids[i]);
                draw_frame(w, ss_win_get_z(win_ids[i]) == highest_active_z);
                memset(w->content_prev, 0xFF, sizeof(w->content_prev));
                draw_content_dirty(w);
            }
            need_full = 0;
        } else if (drag >= 0) {
            /* Self-erasing XOR outline: erase the old rect, draw the new
             * one — and only when the window actually moved this frame. */
            SSWindow* dw = ss_win_get_ptr(win_ids[drag]);
            if (ol_x != dw->x || ol_y != dw->y) {
                ss_gfx_xor_rect(ol_x, ol_y, dw->w, dw->h);
                ol_x = dw->x; ol_y = dw->y;
                ss_gfx_xor_rect(dw->x, dw->y, dw->w, dw->h);
            }
        } else {
            for (int i = 0; i < 3; i++) {
                SSWindow* w = ss_win_get_ptr(win_ids[i]);
                draw_content_dirty(w);
            }
        }

        /* Cooperative: yield to let data_thread run */
        ss_task_yield();
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
        /* Exit supervisor mode via raw trap (see enter comment above) */
        asm volatile(
            "moveq #-127, %%d0\n\t"  /* _B_SUPER = 0x81 */
            "move.l %0, %%d1\n\t"    /* old_ssp */
            "trap #15\n\t"
            :
            : "d"(old_ssp)
            : "d0", "d1"
        );
        _exit(1);
    }

    ss_restore_interrupts();
    ss_restore_trap14();

    *(volatile uint32_t*)0xB0 = saved_copy_vec;
    *(volatile uint32_t*)0x7C = saved_nmi_vec;

    _iocs_ms_curof();
    _iocs_skey_mod(-1, 0, 0);
    _iocs_crtmod(old_mode);
    _iocs_b_curon();

#ifdef DIAG_CASE_1
    /* Case 1: B_SUPER → dos_exit2(0) only */
    asm volatile(
        "moveq #-127, %%d0\n\t"
        "move.l %0, %%d1\n\t"
        "trap #15\n\t"
        : : "d"(old_ssp) : "d0", "d1"
    );
    _dos_exit2(0);

#elif defined(DIAG_CASE_2)
    /* Case 2: B_SUPER → 1000 nops → dos_exit2(0) */
    asm volatile(
        "moveq #-127, %%d0\n\t"
        "move.l %0, %%d1\n\t"
        "trap #15\n\t"
        : : "d"(old_ssp) : "d0", "d1"
    );
    for (volatile int i = 0; i < 1000; i++);
    _dos_exit2(0);

#elif defined(DIAG_CASE_3)
    /* Case 3: B_SUPER → keysns → dos_exit2(0) */
    asm volatile(
        "moveq #-127, %%d0\n\t"
        "move.l %0, %%d1\n\t"
        "trap #15\n\t"
        : : "d"(old_ssp) : "d0", "d1"
    );
    _iocs_b_keysns();
    _dos_exit2(0);

#elif defined(DIAG_CASE_4)
    /* Case 4: B_SUPER → B_PRINT("A") → dos_exit2(0) */
    asm volatile(
        "moveq #-127, %%d0\n\t"
        "move.l %0, %%d1\n\t"
        "trap #15\n\t"
        : : "d"(old_ssp) : "d0", "d1"
    );
    _iocs_b_print("A");
    _dos_exit2(0);

#elif defined(DIAG_CASE_5)
    /* Case 5: B_SUPER → B_PRINT(16 A's) → dos_exit2(0) */
    asm volatile(
        "moveq #-127, %%d0\n\t"
        "move.l %0, %%d1\n\t"
        "trap #15\n\t"
        : : "d"(old_ssp) : "d0", "d1"
    );
    _iocs_b_print("AAAAAAAAAAAAAAAA");
    _dos_exit2(0);

#else
    /* Normal path.
     * B_SUPER MUST come before B_PRINT: issuing IOCS _B_PRINT (trap #15) while
     * still in supervisor mode leaves SSP pointing at the C-runtime stack and
     * disturbs the process state DOS termination walks, triggering an Address
     * Error in the IPL-ROM memmove at 0xFFA870 (move.l -(a0),-(a1), A1 = BSS+1).
     * See commit e8a268e and .ai-handoff.md (s33). All DIAG_CASE_* branches
     * above use this same B_SUPER-first order. */
    asm volatile(
        "moveq #-127, %%d0\n\t"  /* _B_SUPER = 0x81 */
        "move.l %0, %%d1\n\t"    /* old_ssp */
        "trap #15\n\t"
        :
        : "d"(old_ssp)
        : "d0", "d1"
    );
#ifdef SS_BUILD_PREEMPTIVE
    _iocs_b_print("SSOS-Preemptive terminated.\r\n");
#else
    _iocs_b_print("SSOS-Cooperative terminated.\r\n");
#endif
    _exit(0);
#endif
}

#endif /* LOCAL_MODE */
