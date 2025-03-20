#include "ssosmain.h"
#include "kernel.h"
#include "printf.h"
#include "vram.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <x68k/iocs.h>

void draw_background();
void draw_taskbar();
void draw_stats();
void draw_keys();
void ssosmain() {
    // init
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen
    _iocs_g_clr_on(); // clear gvram, reset palette, access page 0
    _iocs_b_curoff(); // stop cursor

    _iocs_ms_init();
    _iocs_skey_mod(0, 0, 0);
    _iocs_ms_curon();

    ss_clear_vram_fast();
    ss_wait_for_clear_vram_completion();

    draw_background();
    draw_taskbar();

#ifdef LOCAL_MODE
    ss_print(5, 0, 0, 0, "Scott & Sandy OS x68k, [ESC] to quit");
#else
    ss_print(5, 0, 0, 0, "Scott & Sandy OS x68k");
#endif

    int c;
    int scancode = 0;
    char szMessage[256];

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
#else
            ;
#endif

        draw_keys();
        draw_stats();
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

void draw_background() { ss_fill_rect(color_bg, 0, 0, WIDTH, HEIGHT - 33); }

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
    uint32_t y = 48;
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
        ss_print(color_fg, color_bg, 0, 16, szMessage);
    }

    if (pos != prev_pos) {
        prev_pos = pos;
        sprintf(szMessage, "mouse x:%3d, y:%3d", (pos & 0xFFFF0000) >> 16,
                pos & 0x0000FFFF);
        ss_print(color_fg, color_bg, 0, 32, szMessage);
    }

    if (counter++ >= 30) {
        counter = 0;

        volatile uint32_t* timera_count = (uint32_t*)0x00300400;
        volatile uint32_t* timerb_count = (uint32_t*)0x00300404;
        volatile uint32_t* timerc_count = (uint32_t*)0x00300408;
        volatile uint32_t* timerd_count = (uint32_t*)0x0030040c;
        volatile uint32_t* key_count = (uint32_t*)0x00300410;

        sprintf(szMessage, "A: V-DISP counter: %9d (vsync count)",
                *timera_count);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        sprintf(szMessage, "C: 100Hz timer:    %9d (every 10ms)",
                *timerc_count);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        sprintf(szMessage, "D: 1000Hz timer:   %9d (every 1ms)", *timerd_count);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

#ifdef LOCAL_MODE
        sprintf(szMessage, "local: %s", local_info);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;
#else
        sprintf(szMessage, ".text: 0x%p-0x%p, size:%d", __text_start,
                __text_end, __text_size);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;
        sprintf(szMessage, ".data: 0x%p-0x%p, size:%d", __data_start,
                __data_end, __data_size);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;
        sprintf(szMessage, ".bss:  0x%p-0x%p, size:%d", __bss_start, __bss_end,
                __bss_size);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        sprintf(szMessage, ".ssos: 0x%p-0x%p, size:%d", __ssosram_start,
                __ssosram_end, __ssosram_size);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;

        sprintf(szMessage, ".app:  0x%p-0x%p, size:%d", __appram_start,
                __appram_end, __appram_size);
        ss_print(color_fg, color_bg, 0, y, szMessage);
        y += 16;
#endif
    }
}

void draw_keys() {
    char szMessage[256];
    int x = 0;
    const int y = 164;

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
