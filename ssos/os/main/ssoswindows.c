#include "ssoswindows.h"
#include "kernel.h"
#include "layer.h"
#include "memory.h"
#include "printf.h"
#include "vram.h"
#include <string.h>
#include <x68k/iocs.h>

extern char local_info[256];

void draw_title(uint8_t* buf);
void draw_taskbar(uint8_t* buf);

Layer* get_layer_1() {
    Layer* l = ss_layer_get();
    uint8_t* lbuf = (uint8_t*)ss_mem_alloc4k(WIDTH * HEIGHT);
    ss_layer_set(l, lbuf, 0, 0, WIDTH, HEIGHT);
    ss_fill_rect_v(lbuf, WIDTH, HEIGHT, color_bg, 0, 0, WIDTH, HEIGHT - 33);

    draw_taskbar(lbuf);
    draw_title(lbuf);
    return l;
}

Layer* get_layer_2() {
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

    // ss_layer_set_z(l, 0);

    return l;
}

Layer* get_layer_3() {
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
}

void update_layer_2(Layer* l) {
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
                 : "=r"(ssp) /* output */
                 :           /* intput */
                 : /* destroyed registers */);
    asm volatile("bsr 1f\n\t"
                 "1: move.l (%%sp)+, %0"
                 : "=r"(pc) /* output */
                 :          /* intput */
                 : /* destroyed registers */);
    asm volatile("move.w %%sr, %0"
                 : "=d"(sr) /* output */
                 :          /* intput */
                 : /* destroyed registers */);

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
}

void update_layer_3(Layer* l) {
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
}

void draw_title(uint8_t* buf) {
#ifdef LOCAL_MODE
    ss_print_v(buf, WIDTH, HEIGHT, 5, 0, 0, 0,
               "Scott & Sandy OS x68k, [ESC] to quit");
#else
    ss_print_v(buf, WIDTH, HEIGHT, 5, 0, 0, 0, "Scott & Sandy OS x68k");
#endif
}

void draw_taskbar(uint8_t* buf) {
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
