#include "ssoswindows.h"
#include "kernel.h"
#include "layer.h"
#include "memory.h"
#include "printf.h"
#include "quickdraw.h"
#include "quickdraw_monitor.h"
#include "quickdraw_shell.h"
#include "ss_perf.h"
#include "vram.h"
#include <string.h>
#include <x68k/iocs.h>

extern char local_info[256];

#if SS_CONFIG_ENABLE_LAYER
static void draw_title(uint8_t* buf);
static void draw_taskbar(uint8_t* buf);
#endif

typedef void (*QuickDrawInitFn)(void);
typedef bool (*QuickDrawUpdateFn)(void);

typedef struct {
    Layer layer_stub;
    QD_Rect clip;
    QD_Rect dirty_rect;
    bool dirty;
    QuickDrawInitFn init_fn;
    QuickDrawUpdateFn update_fn;
    const char* name;
    bool initialized;
    bool always_update;
} SsLayerCompatSurface;

enum {
    SS_LAYER_COMPAT_DESKTOP = 0,
    SS_LAYER_COMPAT_MONITOR = 1,
    SS_LAYER_COMPAT_TASKBAR = 2,
    SS_LAYER_COMPAT_COUNT = 3,
};

static bool qd_monitor_panel_tick_wrapper(void) {
    return qd_monitor_panel_tick();
}

static bool qd_shell_update_desktop_chrome_wrapper(void) {
    qd_shell_update_desktop_chrome();
    return true;
}

static bool qd_shell_update_taskbar_wrapper(void) {
    qd_shell_update_taskbar();
    return true;
}

static SsLayerBackend g_active_backend =
#if SS_CONFIG_ENABLE_LAYER
    SS_LAYER_BACKEND_LEGACY;
#else
    SS_LAYER_BACKEND_QUICKDRAW;
#endif

static SsLayerCompatSurface g_quickdraw_surfaces[SS_LAYER_COMPAT_COUNT] = {
    [SS_LAYER_COMPAT_DESKTOP] = {
        .layer_stub = {
            .x = 0,
            .y = 0,
            .z = SS_LAYER_COMPAT_DESKTOP,
            .w = QD_SCREEN_WIDTH,
            .h = QD_SCREEN_HEIGHT,
            .attr = LAYER_ATTR_VISIBLE,
            .vram = NULL,
            .dirty_x = 0,
            .dirty_y = 0,
            .dirty_w = QD_SCREEN_WIDTH,
            .dirty_h = QD_SCREEN_HEIGHT,
            .needs_redraw = 1,
        },
        .clip = {0, 0, QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT},
        .dirty_rect = {0, 0, 0, 0},
        .dirty = true,
        .init_fn = qd_shell_draw_desktop_chrome,
        .update_fn = qd_shell_update_desktop_chrome_wrapper,
        .name = "DesktopChrome",
        .initialized = false,
        .always_update = false,
    },
    [SS_LAYER_COMPAT_MONITOR] = {
        .layer_stub = {
            .x = QD_MONITOR_PANEL_LEFT,
            .y = QD_MONITOR_PANEL_TOP,
            .z = SS_LAYER_COMPAT_MONITOR,
            .w = QD_MONITOR_PANEL_WIDTH,
            .h = QD_MONITOR_PANEL_HEIGHT,
            .attr = LAYER_ATTR_VISIBLE,
            .vram = NULL,
            .dirty_x = 0,
            .dirty_y = 0,
            .dirty_w = QD_MONITOR_PANEL_WIDTH,
            .dirty_h = QD_MONITOR_PANEL_HEIGHT,
            .needs_redraw = 1,
        },
        .clip = {
            QD_MONITOR_PANEL_LEFT,
            QD_MONITOR_PANEL_TOP,
            QD_MONITOR_PANEL_WIDTH,
            QD_MONITOR_PANEL_HEIGHT,
        },
        .dirty_rect = {0, 0, 0, 0},
        .dirty = true,
        .init_fn = qd_monitor_panel_init,
        .update_fn = qd_monitor_panel_tick_wrapper,
        .name = "MonitorPanel",
        .initialized = false,
        .always_update = true,
    },
    [SS_LAYER_COMPAT_TASKBAR] = {
        .layer_stub = {
            .x = 0,
            .y = (uint16_t)(QD_SCREEN_HEIGHT - QD_SHELL_TASKBAR_HEIGHT),
            .z = SS_LAYER_COMPAT_TASKBAR,
            .w = QD_SCREEN_WIDTH,
            .h = QD_SHELL_TASKBAR_HEIGHT,
            .attr = LAYER_ATTR_VISIBLE,
            .vram = NULL,
            .dirty_x = 0,
            .dirty_y = 0,
            .dirty_w = QD_SCREEN_WIDTH,
            .dirty_h = QD_SHELL_TASKBAR_HEIGHT,
            .needs_redraw = 1,
        },
        .clip = {
            0,
            (int16_t)(QD_SCREEN_HEIGHT - QD_SHELL_TASKBAR_HEIGHT),
            QD_SCREEN_WIDTH,
            QD_SHELL_TASKBAR_HEIGHT,
        },
        .dirty_rect = {0, 0, 0, 0},
        .dirty = true,
        .init_fn = qd_shell_draw_taskbar,
        .update_fn = qd_shell_update_taskbar_wrapper,
        .name = "Taskbar",
        .initialized = false,
        .always_update = false,
    },
};

static void ss_layer_compat_surface_force_dirty(SsLayerCompatSurface* surface) {
    if (!surface) {
        return;
    }

    surface->dirty = true;
    surface->dirty_rect = surface->clip;
    surface->layer_stub.needs_redraw = 1;
    surface->layer_stub.dirty_x = 0;
    surface->layer_stub.dirty_y = 0;
    surface->layer_stub.dirty_w = surface->layer_stub.w;
    surface->layer_stub.dirty_h = surface->layer_stub.h;
}

static void ss_layer_compat_reset_quickdraw_surfaces(void) {
    uint8_t* buffer = qd_get_vram_buffer();
    for (int i = 0; i < SS_LAYER_COMPAT_COUNT; ++i) {
        SsLayerCompatSurface* surface = &g_quickdraw_surfaces[i];
        surface->initialized = false;
        surface->layer_stub.vram = buffer;
        ss_layer_compat_surface_force_dirty(surface);
    }
}

static SsLayerCompatSurface* ss_layer_compat_find_surface(Layer* layer) {
    if (!layer) {
        return NULL;
    }

    for (int i = 0; i < SS_LAYER_COMPAT_COUNT; ++i) {
        SsLayerCompatSurface* surface = &g_quickdraw_surfaces[i];
        if (&surface->layer_stub == layer) {
            return surface;
        }
    }

    return NULL;
}

void ss_layer_compat_on_dirty_marked(Layer* layer) {
    if (!ss_layer_compat_uses_quickdraw() || !layer) {
        return;
    }

    SsLayerCompatSurface* surface = ss_layer_compat_find_surface(layer);
    if (!surface) {
        return;
    }

    if (layer->needs_redraw == 0) {
        surface->dirty = false;
        surface->dirty_rect.x = surface->clip.x;
        surface->dirty_rect.y = surface->clip.y;
        surface->dirty_rect.width = 0;
        surface->dirty_rect.height = 0;
        return;
    }

    surface->dirty = true;

    if (layer->dirty_w == 0 || layer->dirty_h == 0) {
        surface->dirty_rect.x = surface->clip.x;
        surface->dirty_rect.y = surface->clip.y;
        surface->dirty_rect.width = 0;
        surface->dirty_rect.height = 0;
        return;
    }

    int16_t abs_left = (int16_t)(layer->x + layer->dirty_x);
    int16_t abs_top = (int16_t)(layer->y + layer->dirty_y);
    int16_t abs_right = (int16_t)(abs_left + (int16_t)layer->dirty_w);
    int16_t abs_bottom = (int16_t)(abs_top + (int16_t)layer->dirty_h);

    int16_t clip_left = surface->clip.x;
    int16_t clip_top = surface->clip.y;
    int16_t clip_right = (int16_t)(surface->clip.x + (int16_t)surface->clip.width);
    int16_t clip_bottom = (int16_t)(surface->clip.y + (int16_t)surface->clip.height);

    if (abs_left < clip_left) abs_left = clip_left;
    if (abs_top < clip_top) abs_top = clip_top;
    if (abs_right > clip_right) abs_right = clip_right;
    if (abs_bottom > clip_bottom) abs_bottom = clip_bottom;

    if (abs_right <= abs_left || abs_bottom <= abs_top) {
        surface->dirty_rect.x = surface->clip.x;
        surface->dirty_rect.y = surface->clip.y;
        surface->dirty_rect.width = 0;
        surface->dirty_rect.height = 0;
        return;
    }

    surface->dirty_rect.x = abs_left;
    surface->dirty_rect.y = abs_top;
    surface->dirty_rect.width = (uint16_t)(abs_right - abs_left);
    surface->dirty_rect.height = (uint16_t)(abs_bottom - abs_top);
}

void ss_layer_compat_on_layer_cleaned(Layer* layer) {
    if (!ss_layer_compat_uses_quickdraw() || !layer) {
        return;
    }

    SsLayerCompatSurface* surface = ss_layer_compat_find_surface(layer);
    if (!surface) {
        return;
    }

    surface->dirty = false;
    surface->dirty_rect.x = surface->clip.x;
    surface->dirty_rect.y = surface->clip.y;
    surface->dirty_rect.width = 0;
    surface->dirty_rect.height = 0;
}

void ss_layer_compat_select(SsLayerBackend backend) {
#if SS_CONFIG_ENABLE_LAYER
    if (backend == SS_LAYER_BACKEND_LEGACY) {
        g_active_backend = SS_LAYER_BACKEND_LEGACY;
        return;
    }
#endif
    g_active_backend = SS_LAYER_BACKEND_QUICKDRAW;
    ss_layer_compat_reset_quickdraw_surfaces();
}

SsLayerBackend ss_layer_compat_active_backend(void) {
    return g_active_backend;
}

bool ss_layer_compat_uses_quickdraw(void) {
    return g_active_backend == SS_LAYER_BACKEND_QUICKDRAW;
}

static Layer* ss_layer_compat_quickdraw_get(uint8_t index) {
    if (index >= SS_LAYER_COMPAT_COUNT) {
        return NULL;
    }

    SsLayerCompatSurface* surface = &g_quickdraw_surfaces[index];
    surface->layer_stub.vram = qd_get_vram_buffer();

    if (!surface->initialized) {
        if (surface->init_fn) {
            surface->init_fn();
        }
        surface->initialized = true;
        ss_layer_compat_surface_force_dirty(surface);
    }

    return &surface->layer_stub;
}

static void ss_layer_compat_quickdraw_update(uint8_t index) {
    if (index >= SS_LAYER_COMPAT_COUNT) {
        return;
    }

    SsLayerCompatSurface* surface = &g_quickdraw_surfaces[index];
    surface->layer_stub.vram = qd_get_vram_buffer();

    if (!surface->initialized && surface->init_fn) {
        surface->init_fn();
        surface->initialized = true;
        ss_layer_compat_surface_force_dirty(surface);
    }

    bool needs_update = surface->dirty || surface->layer_stub.needs_redraw || surface->always_update;
    if (!surface->update_fn || !needs_update) {
        return;
    }

    const QD_Rect* previous_clip = qd_get_clip_rect();
    QD_Rect saved_clip = *previous_clip;

    QD_Rect clip_region = surface->clip;
    bool has_dirty_rect = surface->dirty_rect.width > 0 && surface->dirty_rect.height > 0;
    if (has_dirty_rect) {
        int16_t left = surface->dirty_rect.x;
        int16_t top = surface->dirty_rect.y;
        int16_t right = (int16_t)(left + (int16_t)surface->dirty_rect.width);
        int16_t bottom = (int16_t)(top + (int16_t)surface->dirty_rect.height);

        int16_t clip_left = surface->clip.x;
        int16_t clip_top = surface->clip.y;
        int16_t clip_right = (int16_t)(surface->clip.x + (int16_t)surface->clip.width);
        int16_t clip_bottom = (int16_t)(surface->clip.y + (int16_t)surface->clip.height);

        if (left < clip_left) left = clip_left;
        if (top < clip_top) top = clip_top;
        if (right > clip_right) right = clip_right;
        if (bottom > clip_bottom) bottom = clip_bottom;

        if (right > left && bottom > top) {
            clip_region.x = left;
            clip_region.y = top;
            clip_region.width = (uint16_t)(right - left);
            clip_region.height = (uint16_t)(bottom - top);
        } else {
            has_dirty_rect = false;
        }
    }

    if (surface->dirty || surface->layer_stub.needs_redraw) {
        qd_set_clip_rect(clip_region.x, clip_region.y, clip_region.width, clip_region.height);
    } else if (surface->always_update) {
        qd_set_clip_rect(surface->clip.x, surface->clip.y, surface->clip.width, surface->clip.height);
    }

    SS_PERF_START_MEASUREMENT(SS_PERF_QD_DRAW_TIME);
    bool updated = surface->update_fn();
    SS_PERF_END_MEASUREMENT(SS_PERF_QD_DRAW_TIME);

    if (updated || surface->dirty || surface->layer_stub.needs_redraw || has_dirty_rect) {
        SS_PERF_INC_GRAPHICS_OPS();
    }

    qd_set_clip_rect(saved_clip.x, saved_clip.y, saved_clip.width, saved_clip.height);

    if (surface->dirty || surface->layer_stub.needs_redraw || updated || has_dirty_rect) {
        surface->layer_stub.needs_redraw = 0;
        surface->layer_stub.dirty_x = 0;
        surface->layer_stub.dirty_y = 0;
        surface->layer_stub.dirty_w = 0;
        surface->layer_stub.dirty_h = 0;
        ss_layer_compat_on_layer_cleaned(&surface->layer_stub);
    }
}

Layer* get_layer_1() {
    if (ss_layer_compat_uses_quickdraw()) {
        return ss_layer_compat_quickdraw_get(SS_LAYER_COMPAT_DESKTOP);
    }
#if SS_CONFIG_ENABLE_LAYER
    Layer* l = ss_layer_get();
    uint8_t* lbuf = (uint8_t*)ss_mem_alloc4k(WIDTH * HEIGHT);
    ss_layer_set(l, lbuf, 0, 0, WIDTH, HEIGHT);
    ss_fill_rect_v(lbuf, WIDTH, HEIGHT, color_bg, 0, 0, WIDTH, HEIGHT - 33);

    draw_taskbar(lbuf);
    draw_title(lbuf);
    return l;
#else
    return NULL;
#endif
}

Layer* get_layer_2() {
    if (ss_layer_compat_uses_quickdraw()) {
        return ss_layer_compat_quickdraw_get(SS_LAYER_COMPAT_MONITOR);
    }
#if SS_CONFIG_ENABLE_LAYER
    uint16_t fg = 0;
    uint16_t bg = 15;

    Layer* l = ss_layer_get();
    const int lw = 512;
    const int lh = 288;

    uint8_t* lbuf = (uint8_t*)ss_mem_alloc4k(lw * lh);
    ss_layer_set(l, lbuf, 16, 80, lw, lh);
    ss_fill_rect_v(lbuf, lw, lh, 2, 0, 0, lw - 1, 24);
    ss_fill_rect_v(lbuf, lw, lh, 15, 0, 25, lw - 1, lh - 1);
    ss_draw_rect_v(lbuf, lw, lh, 0, 0, 0, lw - 1, lh - 1);
    ss_print_v(lbuf, lw, lh, 15, 2, 8, 4, "Every Second: Timer");

    return l;
#else
    return NULL;
#endif
}

Layer* get_layer_3() {
    if (ss_layer_compat_uses_quickdraw()) {
        return ss_layer_compat_quickdraw_get(SS_LAYER_COMPAT_TASKBAR);
    }
#if SS_CONFIG_ENABLE_LAYER
    uint16_t fg = 0;
    uint16_t bg = 15;

    Layer* l = ss_layer_get();
    const int lw = 544;
    const int lh = 96;

    uint8_t* lbuf = (uint8_t*)ss_mem_alloc4k(lw * lh);
    ss_layer_set(l, lbuf, 192, 24, lw, lh);
    ss_fill_rect_v(lbuf, lw, lh, 3, 0, 0, lw - 1, 24);
    ss_fill_rect_v(lbuf, lw, lh, 15, 0, 25, lw - 1, lh - 1);
    ss_draw_rect_v(lbuf, lw, lh, 5, 0, 0, lw - 1, lh - 1);
    ss_print_v(lbuf, lw, lh, 15, 3, 8, 4, "Real Time: Mouse / Keyboard");

    return l;
#else
    return NULL;
#endif
}

void update_layer_2(Layer* l) {
    if (ss_layer_compat_uses_quickdraw()) {
        ss_layer_compat_quickdraw_update(SS_LAYER_COMPAT_MONITOR);
        return;
    }
#if SS_CONFIG_ENABLE_LAYER
    // Static buffers to track previous text - MAJOR PERFORMANCE IMPROVEMENT
    static char prev_timera[256] = {0};
    static char prev_timerd[256] = {0};
    static char prev_global[256] = {0};
    static char prev_context[256] = {0};
    static char prev_ssp[256] = {0};
    static char prev_text[256] = {0};
    static char prev_data[256] = {0};
    static char prev_bss[256] = {0};
    static char prev_ram[256] = {0};
    static char prev_timer_addr[256] = {0};
    static char prev_save_addr[256] = {0};
    static char prev_memory[256] = {0};
    static char prev_blocks[10][256] = {0}; // For memory manager blocks

    char szMessage[256];
    uint16_t fg = 0;
    uint16_t bg = 15;
    uint16_t x = 8;
    uint16_t y = 30;
    int dirty_regions = 0;

    uint8_t lid = l - ss_layer_mgr->layers;
    sprintf(szMessage, "layer id: %d", lid);
    ss_print_v(l->vram, l->w, l->h, fg, bg, x, y, szMessage);
    y += 16;

    sprintf(szMessage, "A: V-DISP counter: %9d (vsync count)", ss_timera_counter);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_timera)) {
        strcpy(prev_timera, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    sprintf(szMessage, "D: 1000Hz timer:   %9d (every 1ms)", ss_timerd_counter);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_timerd)) {
        strcpy(prev_timerd, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    sprintf(szMessage, "global_counter:    %9d (every 1ms)", global_counter);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_global)) {
        strcpy(prev_global, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    sprintf(szMessage, "Context Switch:    %9d (not implemented yet)", ss_context_switch_counter);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_context)) {
        strcpy(prev_context, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    uint32_t ssp;
    uint32_t pc;
    uint16_t sr;
    asm volatile("move.l %%sp, %0"
                 : "=r"(ssp)
                 :
                 :);
    asm volatile("bsr 1f\n\t"
                 "1: move.l (%%sp)+, %0"
                 : "=r"(pc)
                 :
                 :);
    asm volatile("move.w %%sr, %0"
                 : "=d"(sr)
                 :
                 :);

    sprintf(szMessage, "ssp: 0x%08x, pc: 0x%08x, sr: 0x%04x", ssp, pc, sr);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_ssp)) {
        strcpy(prev_ssp, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    void* base;
    uint32_t sz;
    ss_get_text(&base, &sz);
    sprintf(szMessage, ".text   addr: 0x%08x-0x%08x, size: %d", base, base + sz - 1, sz);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_text)) {
        strcpy(prev_text, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    ss_get_data(&base, &sz);
    sprintf(szMessage, ".data   addr: 0x%08x-0x%08x, size: %d", base, base + sz - 1, sz);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_data)) {
        strcpy(prev_data, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    ss_get_bss(&base, &sz);
    sprintf(szMessage, ".bss    addr: 0x%08x-0x%08x, size: %d", base, base + sz - 1, sz);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_bss)) {
        strcpy(prev_bss, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    sprintf(szMessage, "RAM     addr: 0x%08x-0x%08x, size: %d",
            ss_ssos_memory_base, ss_ssos_memory_base + ss_ssos_memory_size - 1, ss_ssos_memory_size);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_ram)) {
        strcpy(prev_ram, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    sprintf(szMessage, "ss_timer_counter_base addr: 0x%p", &ss_timera_counter);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_timer_addr)) {
        strcpy(prev_timer_addr, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    sprintf(szMessage, "ss_save_data_base addr: 0x%p", &ss_save_data_base);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_save_addr)) {
        strcpy(prev_save_addr, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    sprintf(szMessage, "memory total: %d, free: %d", ss_mem_total_bytes(), ss_mem_free_bytes());
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_memory)) {
        strcpy(prev_memory, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    for (int i = 0; i < ss_mem_mgr.num_free_blocks && i < 10; i++) {
        sprintf(szMessage, "memory mgr: block: %d, addr: 0x%x, sz:%d", i,
                ss_mem_mgr.free_blocks[i].addr, ss_mem_mgr.free_blocks[i].sz);
        if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_blocks[i])) {
            strcpy(prev_blocks[i], szMessage);
            ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
            dirty_regions++;
        }
        y += 16;
    }

    // Only invalidate if we actually changed something - MAJOR OPTIMIZATION!
    // This eliminates unnecessary redraws when values haven't changed
#else
    (void)l;
#endif
}

void update_layer_3(Layer* l) {
    if (ss_layer_compat_uses_quickdraw()) {
        ss_layer_compat_quickdraw_update(SS_LAYER_COMPAT_TASKBAR);
        return;
    }
#if SS_CONFIG_ENABLE_LAYER
    // Static variables to track previous values - ELIMINATES REDRAW EVERY FRAME!
    static uint32_t prev_dt = 0;
    static uint32_t prev_pos = 0;
    static char prev_mouse_dt[256] = {0};
    static char prev_mouse_pos[256] = {0};
    static char prev_layer_id[256] = {0};
    static int prev_kb_len = 0;

    char szMessage[256];
    uint16_t fg = 0;
    uint16_t bg = 15;
    uint16_t x = 8;
    uint16_t y = 30;
    int dirty_regions = 0;

    uint8_t lid = l - ss_layer_mgr->layers;
    sprintf(szMessage, "layer id: %d", lid);
    if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_layer_id)) {
        strcpy(prev_layer_id, szMessage);
        ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
        dirty_regions++;
    }
    y += 16;

    // mouse - only update if values actually changed
    uint32_t dt = _iocs_ms_getdt();
    uint32_t pos = _iocs_ms_curgt();

    if (dt != prev_dt) {
        prev_dt = dt;
        int8_t dx = (int8_t)(dt >> 24);
        int8_t dy = (int8_t)((dt & 0xFF0000) >> 16);
        sprintf(szMessage, "mouse dx:%3d, dy:%3d, l-click:%3d, r-click:%3d", dx,
                dy, (dt & 0xFF00) >> 8, dt & 0xFF);
        if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_mouse_dt)) {
            strcpy(prev_mouse_dt, szMessage);
            ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
            dirty_regions++;
        }
    }
    y += 16;

    if (pos != prev_pos) {
        prev_pos = pos;
        sprintf(szMessage, "mouse x:%3d, y:%3d", (pos & 0xFFFF0000) >> 16,
                pos & 0x0000FFFF);
        if (ss_print_v_smart(l->vram, l->w, l->h, fg, bg, x, y, szMessage, prev_mouse_pos)) {
            strcpy(prev_mouse_pos, szMessage);
            ss_layer_mark_dirty(l, x, y, mystrlen(szMessage) * 8, 16);
            dirty_regions++;
        }
    }
    y += 16;

    // keyboard - only update if keyboard buffer length changed
    if (ss_kb.len != prev_kb_len) {
        prev_kb_len = ss_kb.len;

        // Clear the entire keyboard line first
        ss_fill_rect_v(l->vram, l->w, l->h, bg, 8, y, l->w - 8, y + 16);
        ss_layer_mark_dirty(l, 8, y, l->w - 8, 16);
        dirty_regions++;

        if (ss_kb.len > 0) {
            ss_print_v(l->vram, l->w, l->h, fg, bg, x, y, "Code:");
            x += 8 * 5;

            // Create a temporary copy to avoid modifying the original buffer during display
            int temp_len = ss_kb.len;
            int temp_idxr = ss_kb.idxr;

            for (int i = 0; i < temp_len && i < 6; i++) { // Limit to 6 codes to prevent overflow
                int scancode = ss_kb.data[temp_idxr];
                temp_idxr++;
                if (temp_idxr > 32)
                    temp_idxr = 0;
                sprintf(szMessage, " 0x%08x", scancode);
                ss_print_v(l->vram, l->w, l->h, fg, bg, x, y, szMessage);
                x += 8 * 11;
            }

            // Now actually consume the keyboard buffer
            while (ss_kb.len > 0) {
                ss_kb.len--;
                ss_kb.idxr++;
                if (ss_kb.idxr > 32)
                    ss_kb.idxr = 0;
            }
        }
    }

    // The old code ALWAYS invalidated the entire layer - terrible for performance!
    // Now we only mark specific dirty regions when something actually changes
#else
    (void)l;
#endif
}

#if SS_CONFIG_ENABLE_LAYER
static void draw_title(uint8_t* buf) {
#ifdef LOCAL_MODE
    ss_print_v(buf, WIDTH, HEIGHT, 5, 0, 0, 0,
               "Scott & Sandy OS x68k, [ESC] to quit");
#else
    ss_print_v(buf, WIDTH, HEIGHT, 5, 0, 0, 0, "Scott & Sandy OS x68k");
#endif
}

static void draw_taskbar(uint8_t* buf) {
    // white line above the taskbar
    ss_fill_rect_v(buf, WIDTH, HEIGHT, 15, 0, HEIGHT - 33, WIDTH, HEIGHT - 32);
    // taskbar
    ss_fill_rect_v(buf, WIDTH, HEIGHT, color_tb, 0, HEIGHT - 32, WIDTH, HEIGHT);
    // bottom-left button
    // upper -
    ss_fill_rect_v(buf, WIDTH, HEIGHT, 15, 3, HEIGHT - 30, 100, HEIGHT - 30);
    // left |
    ss_fill_rect_v(buf, WIDTH, HEIGHT, 15, 3, HEIGHT - 29, 3, HEIGHT - 3);
    // lower -
    ss_fill_rect_v(buf, WIDTH, HEIGHT, 1, 3, HEIGHT - 3, 100, HEIGHT - 3);
    // right |
    ss_fill_rect_v(buf, WIDTH, HEIGHT, 1, 100, HEIGHT - 29, 100, HEIGHT - 4);
    // bottom-right button
    //  upper -
    ss_fill_rect_v(buf, WIDTH, HEIGHT, 1, WIDTH - 100, HEIGHT - 30, WIDTH - 4,
                   HEIGHT - 30);
    // left |
    ss_fill_rect_v(buf, WIDTH, HEIGHT, 1, WIDTH - 100, HEIGHT - 29, WIDTH - 100,
                   HEIGHT - 3);
    // lower -
    ss_fill_rect_v(buf, WIDTH, HEIGHT, 15, WIDTH - 100, HEIGHT - 3, WIDTH - 4,
                   HEIGHT - 3);
    // right |
    ss_fill_rect_v(buf, WIDTH, HEIGHT, 15, WIDTH - 3, HEIGHT - 29, WIDTH - 3,
                   HEIGHT - 4);
    // start
    ss_print_v(buf, WIDTH, HEIGHT, 1, color_tb, 16, HEIGHT - 24, "Start");
}
#endif
