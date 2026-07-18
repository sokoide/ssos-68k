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
#include "../os/gfx/palette.h"
#include "../os/gfx/profile.h"
#include "../os/win/win.h"
#include "../os/kernel/main_task.h"
#include "../os/app/scene.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../os/util/numfmt.h"

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

#ifdef LOCAL_MODE

static uint8_t local_memory[512 * 1024] __attribute__((aligned(4)));
static uint8_t local_stack_mem[SS_MAX_TASKS * SS_TASK_STACK]
    __attribute__((aligned(4)));

uint8_t* ss_task_stack_base = local_stack_mem;

extern uint32_t ss_context_switch_count;

static void set_palette(void) {
    ss_palette_program_default();
}

#if SS_PROFILE_GFX

/* Palette Indices - Runtime mode-dependent */
static int c_white, c_black, c_gray_l, c_gray_m, c_gray_d;

static void init_palette_indices(void) {
    c_white = ss_palette_index(SS_PALETTE_WHITE);
    c_black = ss_palette_index(SS_PALETTE_BLACK);
    c_gray_l = ss_palette_index(SS_PALETTE_LIGHT_GRAY);
    c_gray_m = ss_palette_index(SS_PALETTE_MEDIUM_GRAY);
    c_gray_d = ss_palette_index(SS_PALETTE_DARK_GRAY);
}

#define C_WHITE c_white
#define C_BLACK c_black
#define C_GRAY_L c_gray_l
#define C_GRAY_M c_gray_m
#define C_GRAY_D c_gray_d

#endif /* SS_PROFILE_GFX */

#if SS_PROFILE_GFX
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
/* Highest other visible window while the dragged one is hidden. */
static uint16_t drag_prev_active;
static int need_full = 1;
static int highest_active_z = -1;

#endif /* SS_PROFILE_GFX */

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

#if SS_PROFILE_GFX

/* Drag outline position tracker (self-erasing XOR rect via ss_gfx_xor_rect).
 * No save buffer: the outline is erased by re-XORing the same rect. */
static int ol_x, ol_y;

/* ---- Window frame drawing ---- */

static void draw_frame(SSWindow* w, int is_fg, const SSGfxRect* clip) {
    uint16_t t_bg = is_fg ? C_GRAY_L : C_WHITE;

    ss_gfx_rect_region((SSGfxRect){w->x + 1, w->y + 1, w->w - 2,
                                   TITLE_H - 2}, clip, t_bg);
    ss_gfx_rect_region((SSGfxRect){w->x + 1, w->y + TITLE_H, w->w - 2,
                                   w->h - TITLE_H - 1}, clip, C_WHITE);

    /* Border lines */
    ss_gfx_rect_region((SSGfxRect){w->x, w->y, w->w, 1}, clip, C_BLACK);
    ss_gfx_rect_region((SSGfxRect){w->x, w->y + w->h - 1, w->w, 1}, clip,
                       C_BLACK);
    ss_gfx_rect_region((SSGfxRect){w->x, w->y + 1, 1, w->h - 2}, clip,
                       C_BLACK);
    ss_gfx_rect_region((SSGfxRect){w->x + w->w - 1, w->y + 1, 1, w->h - 2},
                       clip, C_BLACK);
    ss_gfx_rect_region((SSGfxRect){w->x + 1, w->y + TITLE_H - 1, w->w - 2, 1},
                       clip, C_BLACK);

    int tw = (int)strlen(w->title) * SS_FONT_ADV;
    int tx = w->x + (w->w - tw) / 2;

    if (is_fg) {
        for (int i = 0; i < 5; i++) {
            int ly = w->y + 2 + i * 2;
            if (tx > w->x + 12)
                ss_gfx_rect_region((SSGfxRect){w->x + 4, ly,
                                               tx - 8 - (w->x + 4) + 1, 1},
                                   clip, C_BLACK);
            if (tx + tw + 8 < w->x + w->w - 4)
                ss_gfx_rect_region((SSGfxRect){tx + tw + 8, ly,
                                               (w->x + w->w - 5) -
                                                   (tx + tw + 8) + 1,
                                               1},
                                   clip, C_BLACK);
        }
    }

    if (clip == NULL)
        ss_gfx_draw_text_fast(tx, w->y + 2, w->title, C_BLACK, t_bg);
    else
        ss_gfx_draw_text_region(tx, w->y + 2, w->title, C_BLACK, t_bg, clip);
}

/* ---- Dirty content update ---- */

/* Return visible windows in ascending z-order.  ss_gfx_draw_text_clip()
 * then skips every pixel covered by an entry after target_pos, so an update
 * to a lower window never writes through a higher window on the visible
 * single page. */
static int build_text_clip_windows(SSWindow* target, int clip_wins[3 * 4],
                                   int* target_pos) {
    int order[3];
    int n = 0;

    for (int i = 0; i < 3; i++) {
        SSWindow* w = ss_win_get_ptr(win_ids[i]);
        if (!(w->flags & SS_WIN_VISIBLE)) continue;
        int j = n;
        while (j > 0 && ss_win_get_z(win_ids[order[j - 1]]) > w->z) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = i;
        n++;
    }

    *target_pos = -1;
    for (int i = 0; i < n; i++) {
        SSWindow* w = ss_win_get_ptr(win_ids[order[i]]);
        clip_wins[i * 4] = w->x;
        clip_wins[i * 4 + 1] = w->y;
        clip_wins[i * 4 + 2] = w->w;
        clip_wins[i * 4 + 3] = w->h;
        if (w == target) *target_pos = i;
    }
    return n;
}

static int draw_content_dirty(SSWindow* w) {
    int changed = 0;
    int clip_wins[3 * 4];
    int target_pos;
    int nclip = build_text_clip_windows(w, clip_wins, &target_pos);

    for (int i = 0; i < 3; i++) {
        char current[30];

        /* The data task updates content under the same interrupt guard.
         * Snapshot first, then draw with interrupts enabled; otherwise a
         * Timer D preemption can leave a mixed old/new string in VRAM while
         * content_prev incorrectly records the new string as complete. */
        ss_disable_interrupts();
        memcpy(current, w->content[i], sizeof(current));
        ss_enable_interrupts();

        if (memcmp(current, w->content_prev[i], sizeof(current)) != 0) {
            changed = 1;
            /* Redraw only the changed suffix. Content is space-padded by
             * ss_win_set_content_line, so the first differing column marks
             * the visible change and the padded tail erases any shrink. */
            int j = 0;
            while (j < LINE_LEN && current[j] == w->content_prev[i][j]) j++;

            int x = w->x + 4 + j * SS_FONT_ADV;
            int y = w->y + CONTENT_Y + i * LINE_H;
            int text_w = (LINE_LEN - j - 1) * SS_FONT_ADV + SS_FONT_W;
            int needs_clip = 0;
            for (int k = target_pos + 1; k < nclip; k++) {
                int* upper = &clip_wins[k * 4];
                if (x < upper[0] + upper[2] && x + text_w > upper[0] &&
                    y < upper[1] + upper[3] && y + SS_FONT_H > upper[1]) {
                    needs_clip = 1;
                    break;
                }
            }

            if (target_pos >= 0 && needs_clip) {
                ss_gfx_draw_text_clip(w->x + 4 + j * SS_FONT_ADV,
                                      y, current + j, C_BLACK, C_WHITE,
                                      clip_wins, nclip, target_pos);
            } else {
                ss_gfx_draw_text_fast(x, y, current + j, C_BLACK, C_WHITE);
            }

            /* Do not acknowledge a value that changed while it was drawn.
             * Leaving content_prev untouched schedules one clean redraw on
             * the next frame. */
            ss_disable_interrupts();
            if (memcmp(w->content[i], current, sizeof(current)) == 0)
                memcpy(w->content_prev[i], current, sizeof(current));
            ss_enable_interrupts();
        }
    }
    return changed;
}

/* Recompose the desktop in z-order.  Dragging temporarily hides the moved
 * window; rebuilding from the background avoids copying a stale composite
 * bitmap from the old position into the new position. */
static void redraw_desktop(void) {
    int order[3];
    int n = 0;

    ss_gfx_fill_stipple(0, 0, ss_current_mode->display_w,
                        ss_current_mode->display_h, C_WHITE, C_GRAY_M);

    highest_active_z = -1;
    for (int i = 0; i < 3; i++) {
        SSWindow* w = ss_win_get_ptr(win_ids[i]);
        if ((w->flags & SS_WIN_VISIBLE) && w->z > highest_active_z)
            highest_active_z = w->z;
        int j = n;
        while (j > 0 && ss_win_get_z(win_ids[order[j - 1]]) > w->z) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = i;
        n++;
    }

    for (int k = 0; k < n; k++) {
        SSWindow* w = ss_win_get_ptr(win_ids[order[k]]);
        if (!(w->flags & SS_WIN_VISIBLE)) continue;
        draw_frame(w, w->z == highest_active_z, NULL);
        memset(w->content_prev, 0xFF, sizeof(w->content_prev));
        draw_content_dirty(w);
    }
    need_full = 0;
}

/* Paint only one region. Content is snapshotted per line so a preempting
 * data task cannot make content_prev acknowledge a mixed value. */
static void draw_content_region(SSWindow* w, const SSGfxRect* clip) {
    int x = w->x + 4;
    int line_w = (LINE_LEN - 1) * SS_FONT_ADV + SS_FONT_W;

    for (int i = 0; i < 3; i++) {
        char current[30];
        int y = w->y + CONTENT_Y + i * LINE_H;

        /* Avoid copying and clipping every glyph when this line cannot
         * contribute a pixel to the damage region. */
        if (x >= clip->x + clip->w || x + line_w <= clip->x ||
            y >= clip->y + clip->h || y + SS_FONT_H <= clip->y)
            continue;

        ss_disable_interrupts();
        memcpy(current, w->content[i], sizeof(current));
        ss_enable_interrupts();

        ss_gfx_draw_text_region(x, y, current, C_BLACK, C_WHITE, clip);

        int fully_clipped = x >= clip->x && y >= clip->y &&
                            x + line_w <= clip->x + clip->w &&
                            y + SS_FONT_H <= clip->y + clip->h;
        ss_disable_interrupts();
        if (fully_clipped && memcmp(w->content[i], current, sizeof(current)) == 0)
            memcpy(w->content_prev[i], current, sizeof(current));
        ss_enable_interrupts();
    }
}

static int rect_overlaps_window(const SSGfxRect* clip, const SSWindow* w) {
    return w->x < clip->x + clip->w && w->x + w->w > clip->x &&
           w->y < clip->y + clip->h && w->y + w->h > clip->y;
}

/* Recompose only clip: background first, then overlapping visible windows in
 * ascending z-order so higher windows restore their occlusion naturally. */
static void redraw_region(SSGfxRect clip) {
    int order[3];
    int n = 0;

    ss_gfx_fill_stipple(clip.x, clip.y, clip.w, clip.h, C_WHITE, C_GRAY_M);

    highest_active_z = -1;
    for (int i = 0; i < 3; i++) {
        SSWindow* w = ss_win_get_ptr(win_ids[i]);
        if (!(w->flags & SS_WIN_VISIBLE)) continue;
        if (w->z > highest_active_z) highest_active_z = w->z;
        if (!rect_overlaps_window(&clip, w)) continue;

        int j = n;
        while (j > 0 && ss_win_get_z(win_ids[order[j - 1]]) > w->z) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = i;
        n++;
    }

    for (int k = 0; k < n; k++) {
        SSWindow* w = ss_win_get_ptr(win_ids[order[k]]);
        draw_frame(w, w->z == highest_active_z, &clip);
        draw_content_region(w, &clip);
    }
}

/* A foreground change only alters the title fill and its decorations.  The
 * target window completely covers this strip, so unlike redraw_region there
 * is no exposed background to stipple.  Lower windows are hidden by target;
 * repaint only target and higher overlapping windows in z-order. */
static void redraw_title_region(const SSWindow* w) {
    SSGfxRect clip = {w->x, w->y, w->w, TITLE_H};
    int order[3];
    int n = 0;

    highest_active_z = -1;
    for (int i = 0; i < 3; i++) {
        SSWindow* other = ss_win_get_ptr(win_ids[i]);
        if ((other->flags & SS_WIN_VISIBLE) && other->z > highest_active_z)
            highest_active_z = other->z;
        if (!(other->flags & SS_WIN_VISIBLE) || other->z < w->z ||
            !rect_overlaps_window(&clip, other))
            continue;
        int j = n;
        while (j > 0 && ss_win_get_z(win_ids[order[j - 1]]) > other->z) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = i;
        n++;
    }

    for (int i = 0; i < n; i++) {
        SSWindow* other = ss_win_get_ptr(win_ids[order[i]]);
        draw_frame(other, other->z == highest_active_z, &clip);
        if (other != w) draw_content_region(other, &clip);
    }
}

/* ---- Marching ants outline ---- */

/* The drag outline is now the shared self-erasing ss_gfx_xor_rect — no
 * per-frame ol_save (GVRAM read) / ol_restore / marching-ants redraw.
 * See drag_begin / drag_move / drag_end in the main loop. */

#endif /* SS_PROFILE_GFX */

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

static int scene_wait_vsync(void* ctx) {
    (void)ctx;
    wait_vsync();
    return 0;
}

static int scene_should_stop(void* ctx) {
    (void)ctx;
    return (ss_scene_last_key() & 0xFF) == 0x1B;
}

#if SS_PROFILE_GFX
#define SS_BENCH_DEFAULT_ROUNDS 100U
#define SS_BENCH_MAX_ROUNDS     100000U
#define SS_BENCH_REGION_X       160
#define SS_BENCH_REGION_Y       80
#define SS_BENCH_REGION_W       96
#define SS_BENCH_REGION_H       64
#define SS_BENCH_EXPOSE_X       80
#define SS_BENCH_EXPOSE_Y       60
#define SS_BENCH_EXPOSE_W       360
#define SS_BENCH_EXPOSE_H       140
#define SS_BENCH_OUTLINE_X0     96
#define SS_BENCH_OUTLINE_X1     128
#define SS_BENCH_OUTLINE_Y      220
#define SS_BENCH_DRAG_X0        80
#define SS_BENCH_DRAG_Y0        120
#define SS_BENCH_DRAG_X1        128
#define SS_BENCH_DRAG_Y1        96

static int parse_bench_rounds(const char* text, uint32_t* rounds) {
    uint32_t value = 0;

    if (text == NULL || *text == '\0') return 0;
    for (; *text != '\0'; text++) {
        uint32_t digit;

        if (*text < '0' || *text > '9') return 0;
        digit = (uint32_t)(*text - '0');
        if (value > (SS_BENCH_MAX_ROUNDS - digit) / 10U) return 0;
        value = value * 10U + digit;
    }
    if (value == 0) return 0;

    *rounds = value;
    return 1;
}

static int find_bench_option(int argc, char** argv, uint32_t* rounds) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-bench") != 0) continue;

        *rounds = SS_BENCH_DEFAULT_ROUNDS;
        if (i + 1 < argc) parse_bench_rounds(argv[i + 1], rounds);
        return 1;
    }
    return 0;
}

typedef struct {
    char phase[16];
    uint32_t rounds;
    uint32_t vsyncs;
    SSGfxProfile profile;
} SSBenchResult;

static SSBenchResult bench_results[6];
static uint32_t bench_result_count;
static const SSGfxMode* bench_mode;
static FILE* bench_log_file;

static void bench_print_line(const char* line) {
    _iocs_b_print(line);
    if (bench_log_file != NULL) {
        fputs(line, bench_log_file);
    }
}

static void print_bench_profile(const char* phase, uint32_t rounds,
                                uint32_t vsyncs, const SSGfxMode* mode,
                                const SSGfxProfile* p) {
    char buf[256];

    snprintf(buf, sizeof(buf), "SSPERF phase=%s rounds=%lu vsync=%lu\r\n",
             phase, (unsigned long)rounds, (unsigned long)vsyncs);
    bench_print_line(buf);
    snprintf(buf, sizeof(buf),
             "SSPERF mode crtmod=%d display=%dx%d color=%d pages=%d\r\n",
             mode->crtmod, mode->display_w, mode->display_h,
             mode->color_count, mode->page_count);
    bench_print_line(buf);
    snprintf(buf, sizeof(buf),
             "SSPERF calls primitive=%lu rect=%lu hline=%lu stipple=%lu xor=%lu\r\n",
             (unsigned long)p->primitive_calls, (unsigned long)p->rect_calls,
             (unsigned long)p->hline_calls, (unsigned long)p->stipple_calls,
             (unsigned long)p->xor_rect_calls);
    bench_print_line(buf);
    snprintf(buf, sizeof(buf),
             "SSPERF glyph slow=%lu fast=%lu clip=%lu text=%lu\r\n",
             (unsigned long)p->glyph_slow, (unsigned long)p->glyph_fast,
             (unsigned long)p->glyph_clip, (unsigned long)p->text_calls);
    bench_print_line(buf);
    snprintf(buf, sizeof(buf),
             "SSPERF gvram read=%lu write=%lu area=%lu clipped=%lu\r\n",
             (unsigned long)p->gvram_words_read,
             (unsigned long)p->gvram_words_written,
             (unsigned long)p->submitted_area,
             (unsigned long)p->clipped_area);
    bench_print_line(buf);
    snprintf(buf, sizeof(buf),
             "SSPERF dma attempts=%lu ok=%lu error=%lu timeout=%lu fallback_rows=%lu status_samples=%lu csr=%02X cer=%02X config_samples=%lu dcr=%02X ocr=%02X scr=%02X mfc=%02X dfc=%02X bfc=%02X\r\n",
             (unsigned long)p->dma_attempts, (unsigned long)p->dma_ok,
             (unsigned long)p->dma_error, (unsigned long)p->dma_timeout,
             (unsigned long)p->dma_fallback_rows,
             (unsigned long)p->dma_error_status_samples,
             (unsigned)p->dma_last_csr, (unsigned)p->dma_last_cer,
             (unsigned long)p->dma_config_samples,
             (unsigned)p->dma_dcr, (unsigned)p->dma_ocr, (unsigned)p->dma_scr,
             (unsigned)p->dma_mfc, (unsigned)p->dma_dfc, (unsigned)p->dma_bfc);
    bench_print_line(buf);
    snprintf(buf, sizeof(buf),
             "SSPERF render all=%lu region=%lu background=%lu zmap=%lu\r\n",
             (unsigned long)p->render_all_calls,
             (unsigned long)p->render_region_calls,
             (unsigned long)p->full_background_fills,
             (unsigned long)p->zmap_rebuilds);
    bench_print_line(buf);
    snprintf(buf, sizeof(buf),
             "SSPERF windows considered=%lu rendered=%lu skip_overlap=%lu skip_occluded=%lu\r\n",
             (unsigned long)p->windows_considered,
             (unsigned long)p->windows_rendered,
             (unsigned long)p->windows_skipped_no_overlap,
             (unsigned long)p->windows_skipped_occluded);
    bench_print_line(buf);
    snprintf(buf, sizeof(buf),
             "SSPERF dirty marks=%lu submitted=%lu clipped=%lu\r\n",
             (unsigned long)p->dirty_marks,
             (unsigned long)p->dirty_area_submitted,
             (unsigned long)p->dirty_area_clipped);
    bench_print_line(buf);
    snprintf(buf, sizeof(buf), "SSPERF drag save_words=%lu restore_words=%lu\r\n",
             (unsigned long)p->drag_save_words,
             (unsigned long)p->drag_restore_words);
    bench_print_line(buf);
}

static void bench_full_render(uint32_t rounds) {
    for (uint32_t i = 0; i < rounds; i++) ss_win_render_all();
}

static void bench_small_region(uint32_t rounds) {
    for (uint32_t i = 0; i < rounds; i++) {
        ss_win_render_region(SS_BENCH_REGION_X, SS_BENCH_REGION_Y,
                             SS_BENCH_REGION_W, SS_BENCH_REGION_H);
    }
}

static void bench_z_exposure(uint32_t rounds) {
    for (uint32_t i = 0; i < rounds; i++) {
        if (i & 1U) {
            ss_win_set_z(win_ids[1], 3);
            ss_win_set_z(win_ids[2], 2);
        } else {
            ss_win_set_z(win_ids[1], 2);
            ss_win_set_z(win_ids[2], 3);
        }
        ss_win_render_region(SS_BENCH_EXPOSE_X, SS_BENCH_EXPOSE_Y,
                             SS_BENCH_EXPOSE_W, SS_BENCH_EXPOSE_H);
    }
    ss_win_set_z(win_ids[1], 2);
    ss_win_set_z(win_ids[2], 3);
}

static void bench_xor_outline(uint32_t rounds) {
    int x = SS_BENCH_OUTLINE_X0;

    ss_gfx_xor_rect(x, SS_BENCH_OUTLINE_Y, WIN_W, WIN_H);
    for (uint32_t i = 0; i < rounds; i++) {
        ss_gfx_xor_rect(x, SS_BENCH_OUTLINE_Y, WIN_W, WIN_H);
        x = (x == SS_BENCH_OUTLINE_X0) ? SS_BENCH_OUTLINE_X1
                                        : SS_BENCH_OUTLINE_X0;
        ss_gfx_xor_rect(x, SS_BENCH_OUTLINE_Y, WIN_W, WIN_H);
    }
    ss_gfx_xor_rect(x, SS_BENCH_OUTLINE_Y, WIN_W, WIN_H);
}

static void bench_text_update(uint32_t rounds) {
    static const char text_a[] = "Vsync: 00000000             ";
    static const char text_b[] = "Vsync: 11111111             ";

    for (uint32_t i = 0; i < rounds; i++) {
        ss_gfx_draw_text_fast(34, 34, (i & 1U) ? text_a : text_b,
                              C_BLACK, C_WHITE);
    }
}

static SSWindow bench_drag_saved[3];
static int bench_drag_saved_highest_active_z;

/* Set up a fixed scene outside the profile interval. */
static void bench_drag_region_prepare(void) {
    SSWindow* dragged = ss_win_get_ptr(win_ids[2]);

    ss_disable_interrupts();
    for (int i = 0; i < 3; i++)
        memcpy(&bench_drag_saved[i], ss_win_get_ptr(win_ids[i]),
               sizeof(bench_drag_saved[i]));
    bench_drag_saved_highest_active_z = highest_active_z;
    ss_enable_interrupts();

    ss_win_move(dragged->id, SS_BENCH_DRAG_X0, SS_BENCH_DRAG_Y0);
    ss_win_show(dragged->id);
    ss_win_set_z(dragged->id, 4);
    redraw_desktop();
}

/* Exercise the same hide/XOR/move/show region path as a drag.  The fixed
 * starting scene is prepared before the profile is reset, so the result is
 * only the cost of the repeated drag path. */
static void bench_drag_region(uint32_t rounds) {
    SSWindow* dragged = ss_win_get_ptr(win_ids[2]);
    SSWindow* previous = ss_win_get_ptr(win_ids[1]);
    SSGfxRect base = {SS_BENCH_DRAG_X0, SS_BENCH_DRAG_Y0, WIN_W, WIN_H};
    SSGfxRect moved = {SS_BENCH_DRAG_X1, SS_BENCH_DRAG_Y1, WIN_W, WIN_H};

    for (uint32_t i = 0; i < rounds; i++) {
        SSGfxRect old_rect = (i & 1U) ? moved : base;
        SSGfxRect new_rect = (i & 1U) ? base : moved;

        ss_win_hide(dragged->id);
        redraw_region(old_rect);
        redraw_title_region(previous);
        ss_gfx_xor_rect(old_rect.x, old_rect.y, old_rect.w, old_rect.h);
        ss_gfx_xor_rect(old_rect.x, old_rect.y, old_rect.w, old_rect.h);
        ss_win_move(dragged->id, new_rect.x, new_rect.y);
        ss_win_show(dragged->id);
        redraw_region(new_rect);
        redraw_title_region(previous);
    }

}

/* Restore outside the profile interval.  redraw_desktop updates content_prev,
 * so copy the exact model one final time after repainting. */
static void bench_drag_region_restore(void) {
    ss_disable_interrupts();
    for (int i = 0; i < 3; i++)
        memcpy(ss_win_get_ptr(win_ids[i]), &bench_drag_saved[i],
               sizeof(bench_drag_saved[i]));
    ss_enable_interrupts();
    redraw_desktop();
    ss_disable_interrupts();
    for (int i = 0; i < 3; i++)
        memcpy(ss_win_get_ptr(win_ids[i]), &bench_drag_saved[i],
               sizeof(bench_drag_saved[i]));
    highest_active_z = bench_drag_saved_highest_active_z;
    ss_enable_interrupts();
}

typedef void (*SSBenchPhase)(uint32_t rounds);

static void run_bench_phase(const char* name, uint32_t rounds,
                            SSBenchPhase phase) {
    SSBenchResult* result;
    uint32_t start_vsync;

    if (bench_result_count >= 6) return;
    result = &bench_results[bench_result_count++];
    strncpy(result->phase, name, sizeof(result->phase) - 1);
    result->phase[sizeof(result->phase) - 1] = '\0';
    result->rounds = rounds;
    ss_gfx_profile_reset();
    start_vsync = ss_vsync_counter;
    phase(rounds);
    result->vsyncs = ss_vsync_counter - start_vsync;
    ss_gfx_profile_snapshot(&result->profile);
}

static void run_benchmark(uint32_t rounds) {
    bench_result_count = 0;
    bench_mode = ss_current_mode;
    run_bench_phase("full", rounds, bench_full_render);
    run_bench_phase("region", rounds, bench_small_region);
    run_bench_phase("z-expose", rounds, bench_z_exposure);
    run_bench_phase("text-update", rounds, bench_text_update);
    bench_drag_region_prepare();
    run_bench_phase("drag-region", rounds, bench_drag_region);
    bench_drag_region_restore();
    run_bench_phase("xor-move", rounds, bench_xor_outline);
}

static void print_bench_results(void) {
    for (uint32_t i = 0; i < bench_result_count; i++) {
        SSBenchResult* result = &bench_results[i];
        print_bench_profile(result->phase, result->rounds, result->vsyncs,
                            bench_mode, &result->profile);
    }
}

#endif /* SS_PROFILE_GFX */

/* ================================================================
 * Main
 * ================================================================ */

int main(int argc, char** argv) {
    /* Parse command line arguments for graphics mode */
    int requested_mode = SS_CRTMOD_16;  /* Default: mode 16 */
#if SS_PROFILE_GFX
    uint32_t bench_rounds;
    int run_bench = find_bench_option(argc, argv, &bench_rounds);
#endif
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-8") == 0) {
            requested_mode = SS_CRTMOD_8;
        } else if (strcmp(argv[i], "-16") == 0) {
            requested_mode = SS_CRTMOD_16;
        }
    }
    ss_gfx_set_mode(requested_mode);
#if SS_PROFILE_GFX
    init_palette_indices();
#endif

    /*
     * Enter Supervisor mode via IOCS B_SUPER(0). Returns the previous USP
     * (A7 in user mode), saved to restore user mode on exit.
     *
     * _iocs_b_super is safe straight from user mode: the libx68k wrapper
     * (b_super.S) only runs `move.l %sp,%usp` when MD != 0 — with MD = 0 the
     * `beq` skips it. (MD < 0 would make IOCS derive SSP from MD and corrupt
     * the stack on entry — never pass a negative MD.)
     */
    int old_usp = _iocs_b_super(0);

    ss_init_trap14();

    ss_mem_init(local_memory, sizeof(local_memory));
    ss_sched_init();

    if (ss_main_task_register(&main_tcb, 8) != SS_OK) _exit(1);

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

#if SS_PROFILE_GFX
    if (run_bench) {
        for (int i = 0; i < SS_SCENE_WINDOW_COUNT; i++) {
            const SSSceneWindowSpec* spec = &ss_scene_default_windows[i];
            win_ids[i] = ss_win_create(spec->x, spec->y,
                                       spec->w, spec->h, spec->z);
            ss_win_set_title(win_ids[i], spec->title);
        }
        run_benchmark(bench_rounds);
        goto cleanup;
    }
#endif

#if SS_PROFILE_GFX
    SSGfxProfile runtime_profile;
    ss_gfx_profile_reset();
#endif
    SSSceneStats runtime_stats;
    SSSceneHooks scene_hooks = {
        .wait_vsync = scene_wait_vsync,
        .should_stop = scene_should_stop,
        .ctx = NULL,
    };
    ss_scene_run(&scene_hooks, &runtime_stats);

cleanup:
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
        /* Return to user mode. _iocs_b_super sets a1 = MD internally, so the
         * a1 scratch left by the _iocs_b_print calls above is harmless. */
        _iocs_b_super(old_usp);
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

#if SS_PROFILE_GFX
    /* CRTMOD restoration clears the graphics page.  Emit retained benchmark
     * records only after returning to the text console so the user can read
     * and capture them from the emulator terminal. */
    if (run_bench) {
        bench_log_file = fopen("bench.txt", "w");
        if (bench_log_file == NULL) {
            _iocs_b_print("SSPERF file=open-failed name=bench.txt\r\n");
        }
        print_bench_results();
        if (bench_log_file != NULL) {
            fclose(bench_log_file);
            bench_log_file = NULL;
            _iocs_b_print("SSPERF file=bench.txt\r\n");
        }
    } else {
        ss_gfx_profile_snapshot(&runtime_profile);
        bench_log_file = fopen("runtime.txt", "w");
        if (bench_log_file == NULL) {
            _iocs_b_print("SSPERF file=open-failed name=runtime.txt\r\n");
        }
        print_bench_profile("runtime", runtime_stats.frames,
                            runtime_stats.vsyncs,
                            ss_current_mode, &runtime_profile);
        if (bench_log_file != NULL) {
            fclose(bench_log_file);
            bench_log_file = NULL;
            _iocs_b_print("SSPERF file=runtime.txt\r\n");
        }
    }
#endif

    /* Normal exit. Print the terminated message while in supervisor mode, then
     * return to user mode via B_SUPER(old_usp) and _exit. _iocs_b_super sets
     * a1 = MD internally, so the a1 scratch left by the _iocs_b_print above is
     * harmless — this is exactly what the old raw-trap workaround had to do by
     * hand (the s33 B_PRINT/USP crash was caused by a raw trap reusing a stale
     * a1; the wrapper avoids it by writing a1 = MD itself). */
#ifdef SS_BUILD_PREEMPTIVE
    _iocs_b_print("SSOS-Preemptive terminated.\r\n");
#else
    _iocs_b_print("SSOS-Cooperative terminated.\r\n");
#endif
    _iocs_b_super(old_usp);
    _exit(0);
}

#endif /* LOCAL_MODE */
