#include "ssosmain.h"
#include "crtc.h"
#include "kernel.h"
#include "printf.h"
#include "vram.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <x68k/iocs.h>

const int WIDTH = 758;
const int HEIGHT = 512;
uint16_t color_fg = 15; // foreground color
uint16_t color_bg = 10; // background color
uint16_t color_tb = 14; // taskbar color

void demo();
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
                goto CLEANUP;
        }
        // wait for vsync
        while ((*mfp) & 0x10) {
            if (ss_handle_keys() == -1)
                goto CLEANUP;
        }

        if (ss_handle_keys() == -1)
            break;

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

void demo() {
    int c = 0;
    int scancode = 0;
    uint8_t counter = 0;
    char szMessage[256];

    /* _iocs_b_print("Hello\r\n"); */

    for (int i = 0; i < 16; i++) {
        ss_fill_rect(i, 100 + 20 * i, 100 + 20 * i, 200 + 20 * i, 200 + 20 * i);
    }
#ifdef LOCAL_MODE
    ss_print(15, 0, 0, 0, "Scott & Sandy OS x68k, [ESC] to quit");
#else
    ss_print(15, 0, 0, 0, "Scott & Sandy OS x68k");
#endif
    for (int i = 0; i < 64; i++) {
        int fg = i % 16;
        int bg = fg ? 0 : 15;
        ss_put_char(fg, bg, 8 * i, 200, i);
        ss_put_char(fg, bg, 8 * i, 216, i + 64);
    }

    uint32_t prev_dt = 0;
    uint32_t prev_pos = 0;

    while (1) {
        ss_wait_for_vsync();

        // key
        c = _iocs_b_keysns();
        if (c > 0) {
            // delete the key from the buffer
            scancode = _iocs_b_keyinp();
            sprintf(szMessage, "Scan Code:        0x%08x", scancode);
            ss_print(15, 0, 0, 100, szMessage);
#ifdef LOCAL_MODE
            if ((scancode & 0xFFFF) == 0x011b) {
                // ESC key
                break;
            }
#endif
        }

        // mouse
        uint32_t dt = _iocs_ms_getdt();
        uint32_t pos = _iocs_ms_curgt();

        if (dt != prev_dt) {
            prev_dt = dt;
            int8_t dx = (int8_t)(dt >> 24);
            int8_t dy = (int8_t)((dt & 0xFF0000) >> 16);
            sprintf(szMessage, "mouse dx:%3d, dy:%3d, l-click:%3d, r-click:%3d",
                    dx, dy, (dt & 0xFF00) >> 8, dt & 0xFF);
            ss_print(7, 0, 0, 16, szMessage);
        }

        if (pos != prev_pos) {
            prev_pos = pos;
            sprintf(szMessage, "mouse x:%3d, y:%3d", (pos & 0xFFFF0000) >> 16,
                    pos & 0x0000FFFF);
            ss_print(8, 0, 0, 32, szMessage);
        }

        if (counter++ >= 30) {
            counter = 0;
            uint32_t y = 48;

            volatile uint32_t* timera_count = (uint32_t*)0x00300400;
            volatile uint32_t* timerb_count = (uint32_t*)0x00300404;
            volatile uint32_t* timerc_count = (uint32_t*)0x00300408;
            volatile uint32_t* timerd_count = (uint32_t*)0x0030040c;
            volatile uint32_t* key_count = (uint32_t*)0x00300410;

            sprintf(szMessage, "A: V-DISP counter: %9d (vsync count)",
                    *timera_count);
            ss_print(9, 0, 0, y, szMessage);
            y += 16;

            // timer B is used by IOCS's key handler
            /* sprintf(szMessage, "B: timer:         %9d", *timerb_count);
             */
            /* ss_print(9, 0, 0, y, szMessage); */
            /* y += 16; */

            sprintf(szMessage, "C: 100Hz timer:    %9d (every 10ms)",
                    *timerc_count);
            ss_print(9, 0, 0, y, szMessage);
            y += 16;

            sprintf(szMessage, "D: 1000Hz timer:   %9d (every 1ms)",
                    *timerd_count);
            ss_print(9, 0, 0, y, szMessage);
            y += 16;

            // we are using IOCS's key buffer (e.g. iocs_b_keysns/inp)
            // if you prefer your key interrupt, enable usart_handler in
            // entry.s and implement the key receive buffer
            /* sprintf(szMessage, "Key:             %9d", *key_count); */
            /* ss_print(9, 0, 0, y, szMessage); */
            /* y += 16; */
        }
    }
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
    }
}

void draw_keys() {
    char szMessage[256];
    int x = 0;

    if (kb.len == 0)
        return;

    ss_print(color_fg, color_bg, x, 100, "ScanCode:");
    x += 8 * 9;

    while (kb.len > 0) {
        int scancode = kb.data[kb.idxr];
        kb.len--;
        kb.idxr++;
        if (kb.idxr > 32)
            kb.idxr = 0;
        sprintf(szMessage, " 0x%08x", scancode);
        ss_print(color_fg, color_bg, x, 100, szMessage);
        x += 8 * 11;
    }
    // clear
    ss_fill_rect(color_bg, x, 100, 768, 116);
}
