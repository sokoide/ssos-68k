#include "ssosmain.h"
#include "kernel.h"
#include "layer.h"
#include "memory.h"
#include "printf.h"
#include "vram.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x68k/iocs.h>

void draw_background();
void draw_taskbar();
void draw_stats();
void draw_keys();

extern char local_info[256];

void ssosmain() {
    int c;
    int scancode = 0;
    char szMessage[256];
    uint8_t counter = 0;

    // init
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen
    _iocs_g_clr_on(); // clear gvram, reset palette, access page 0
    _iocs_b_curoff(); // stop cursor

    _iocs_ms_init();
    _iocs_skey_mod(0, 0, 0);
    _iocs_ms_curon();

    ss_clear_vram_fast();
    ss_wait_for_clear_vram_completion();

    ss_init_memory_info();
    ss_mem_init();

    ss_layer_init();

    Layer* l1 = ss_layer_get();
    uint16_t* l1buf = (uint16_t*)ss_mem_alloc4k(WIDTH * HEIGHT * 2);
    ss_layer_set(l1, l1buf, 0, 0, WIDTH, HEIGHT);
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, color_bg, 0, 0, WIDTH, HEIGHT - 33);
    // white line above the taskbar
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, 15, 0, HEIGHT - 33, WIDTH,
                   HEIGHT - 32);
    // taskbar
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, color_tb, 0, HEIGHT - 32, WIDTH,
                   HEIGHT);
    // bottom-left button
    // upper -
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, 15, 3, HEIGHT - 30, 100, HEIGHT - 30);
    // left |
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, 15, 3, HEIGHT - 29, 3, HEIGHT - 3);
    // lower -
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, 1, 3, HEIGHT - 3, 100, HEIGHT - 3);
    // right |
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, 1, 100, HEIGHT - 29, 100, HEIGHT - 4);
    // bottom-right button
    //  upper -
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, 1, WIDTH - 100, HEIGHT - 30, WIDTH - 4,
                   HEIGHT - 30);
    // left |
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, 1, WIDTH - 100, HEIGHT - 29,
                   WIDTH - 100, HEIGHT - 3);
    // lower -
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, 15, WIDTH - 100, HEIGHT - 3, WIDTH - 4,
                   HEIGHT - 3);
    // right |
    ss_fill_rect_v(l1buf, WIDTH, HEIGHT, 15, WIDTH - 3, HEIGHT - 29, WIDTH - 3,
                   HEIGHT - 4);
    // start
    ss_print_v(l1buf, WIDTH, HEIGHT, 1, color_tb, 16, HEIGHT - 24, "Start");
#ifdef LOCAL_MODE
    ss_print_v(l1buf, WIDTH, HEIGHT, 5, 0, 0, 0,
               "Scott & Sandy OS x68k, [ESC] to quit");
#else
    ss_print_v(l1buf, WIDTH, HEIGHT, 5, 0, 0, 0, "Scott & Sandy OS x68k");
#endif

    Layer* l2 = ss_layer_get();
    const int l2w = 400;
    const int l2h = 64;
    uint16_t* l2buf = (uint16_t*)ss_mem_alloc4k(l2w * l2h * 2);
    ss_layer_set(l2, l2buf, 300, 40, l2w, l2h);
    ss_fill_rect_v(l2buf, l2w, l2h, 2, 0, 0, l2w, 24);
    ss_fill_rect_v(l2buf, l2w, l2h, 15, 0, 25, l2w, l2h);
    ss_draw_rect_v(l2buf, l2w, l2h, 0, 0, 0, l2w, l2h);
    ss_print_v(l2buf, l2w, l2h, 15, 2, 8, 4, "Timer");
    sprintf(szMessage, "A: V-DISP counter: %9d (vsync count)",
            ss_timera_counter);
    ss_print_v(l2buf, l2w, l2h, 0, 15, 8, 30, szMessage);
    sprintf(szMessage, "D: 1000Hz timer:   %9d (every 1ms)", ss_timerd_counter);
    ss_print_v(l2buf, l2w, l2h, 0, 15, 8, 46, szMessage);
    // ss_layer_set_z(l1, 0);

    // draw_background();
    // draw_taskbar();

    ss_all_layer_draw();

    while (true) {
        // if it's vsync, wait for display period
        while (!((*mfp) & 0x10)) {
            if (ss_handle_keys() == -1)
#ifdef LOCAL_MODE
                goto CLEANUP;
#else
                ;
#endif
        }
        // wait for vsync
        while ((*mfp) & 0x10) {
            if (ss_handle_keys() == -1)
#ifdef LOCAL_MODE
                goto CLEANUP;
#else
                ;
#endif
        }

        if (ss_handle_keys() == -1)
#ifdef LOCAL_MODE
            break;

        ;
#endif

        counter++;
        if (counter % 30 == 0) {
            // if (l2->y < 400)
            //     ss_layer_move(l2, l2->x, l2->y + 10);
            // else
            //     ss_layer_move(l2, l2->x, 40);
            sprintf(szMessage, "A: V-DISP counter: %9d (vsync count)",
                    ss_timera_counter);
            ss_print_v(l2buf, l2w, l2h, 0, 15, 8, 30, szMessage);
            sprintf(szMessage, "D: 1000Hz timer:   %9d (every 1ms)",
                    ss_timerd_counter);
            ss_print_v(l2buf, l2w, l2h, 0, 15, 8, 46, szMessage);
            ss_layer_invalidate(l2);
        }
        if (counter > 60) {
            counter = 0;
        }
        // draw_keys();
        // draw_stats();
    }

CLEANUP:
    // uninit
    _iocs_ms_curof();
    _iocs_skey_mod(-1, 0, 0);
    _iocs_b_curon();  // enable cursor
    _iocs_g_clr_on(); // clear graphics, reset palette to the default,
                      // access page 0
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen
}

void draw_background() {
    ss_fill_rect(color_bg, 0, 0, WIDTH, HEIGHT - 33);
#ifdef LOCAL_MODE
    ss_print(5, 0, 0, 0, "Scott & Sandy OS x68k, [ESC] to quit");
#else
    ss_print(5, 0, 0, 0, "Scott & Sandy OS x68k");
#endif
}

void draw_taskbar() {
    // white line above the taskbar
    ss_fill_rect(15, 0, HEIGHT - 33, WIDTH, HEIGHT - 32);

    // taskbar
    ss_fill_rect(color_tb, 0, HEIGHT - 32, WIDTH, HEIGHT);

    // bottom-left button
    // uppper -
    ss_fill_rect(15, 3, HEIGHT - 30, 100, HEIGHT - 30);
    // left |
    ss_fill_rect(15, 3, HEIGHT - 29, 3, HEIGHT - 3);
    // lower -
    ss_fill_rect(1, 3, HEIGHT - 3, 100, HEIGHT - 3);
    // right |
    ss_fill_rect(1, 100, HEIGHT - 29, 100, HEIGHT - 4);

    // bottom-right button
    //  upper -
    ss_fill_rect(1, WIDTH - 100, HEIGHT - 30, WIDTH - 4, HEIGHT - 30);
    // left |
    ss_fill_rect(1, WIDTH - 100, HEIGHT - 29, WIDTH - 100, HEIGHT - 3);
    // lower -
    ss_fill_rect(15, WIDTH - 100, HEIGHT - 3, WIDTH - 4, HEIGHT - 3);
    // right |
    ss_fill_rect(15, WIDTH - 3, HEIGHT - 29, WIDTH - 3, HEIGHT - 4);

    // start
    ss_print(1, color_tb, 16, HEIGHT - 24, "Start");
}

void draw_stats() {
    static uint8_t counter = 0;
    const uint32_t oy = 32;
    uint32_t y = oy + 32;
    char szMessage[256];
    uint32_t prev_dt = 0;
    uint32_t prev_pos = 0;

    // mouse
    uint32_t dt = _iocs_ms_getdt();
    uint32_t pos = _iocs_ms_curgt();

    if (dt != prev_dt) {
        prev_dt = dt;
        int8_t dx = (int8_t)(dt >> 24);
        int8_t dy = (int8_t)((dt & 0xFF0000) >> 16);
        sprintf(szMessage, "mouse dx:%3d, dy:%3d, l-click:%3d, r-click:%3d", dx,
                dy, (dt & 0xFF00) >> 8, dt & 0xFF);
        ss_print(color_fg, color_bg, 0, oy, szMessage);
    }

    if (pos != prev_pos) {
        prev_pos = pos;
        sprintf(szMessage, "mouse x:%3d, y:%3d", (pos & 0xFFFF0000) >> 16,
                pos & 0x0000FFFF);
        ss_print(color_fg, color_bg, 0, oy + 16, szMessage);
    }

    if (counter++ >= 60) {
        counter = 0;

        sprintf(szMessage, "A: V-DISP counter: %9d (vsync count)",
                ss_timera_counter);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        sprintf(szMessage, "D: 1000Hz timer:   %9d (every 1ms)",
                ss_timerd_counter);
        ss_print(color_fg, color_bg, 0, y, szMessage);
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
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        void* base;
        uint32_t sz;
        ss_get_text(&base, &sz);
        sprintf(szMessage, ".text   addr: 0x%08x-0x%08x, size: %d", base,
                base + sz - 1, sz);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        ss_get_data(&base, &sz);
        sprintf(szMessage, ".data   addr: 0x%08x-0x%08x, size: %d", base,
                base + sz - 1, sz);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        ss_get_bss(&base, &sz);
        sprintf(szMessage, ".bss    addr: 0x%08x-0x%08x, size: %d", base,
                base + sz - 1, sz);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        sprintf(szMessage, "RAM     addr: 0x%08x-0x%08x, size: %d",
                ss_ssos_memory_base,
                ss_ssos_memory_base + ss_ssos_memory_size - 1,
                ss_ssos_memory_size);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        sprintf(szMessage, "ss_timer_counter_base addr: 0x%p",
                &ss_timera_counter);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        sprintf(szMessage, "ss_save_data_base addr: 0x%p", &ss_save_data_base);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        sprintf(szMessage, "memory total: %d, free: %d", ss_mem_total_bytes(),
                ss_mem_free_bytes());
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        for (int i = 0; i < ss_mem_mgr.num_free_blocks; i++) {
            sprintf(szMessage, "memory mgr: block: %d, addr: 0x%x, sz:%d", i,
                    ss_mem_mgr.free_blocks[i].addr,
                    ss_mem_mgr.free_blocks[i].sz);
            ss_print(color_fg, color_bg, 0, y, szMessage);
            y += 16;
        }
    }
}

void draw_keys() {
    char szMessage[256];
    int x = 0;
    const int y = 16;

    if (ss_kb.len == 0)
        return;

    ss_print(color_fg, color_bg, x, y, "ScanCode:");
    x += 8 * 9;

    while (ss_kb.len > 0) {
        int scancode = ss_kb.data[ss_kb.idxr];
        ss_kb.len--;
        ss_kb.idxr++;
        if (ss_kb.idxr > 32)
            ss_kb.idxr = 0;
        sprintf(szMessage, " 0x%08x", scancode);
        ss_print(color_fg, color_bg, x, y, szMessage);
        x += 8 * 11;
    }
    // clear
    ss_fill_rect(color_bg, x, y, WIDTH, y + 16);
}
