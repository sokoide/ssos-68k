#include "ssosmain.h"
#include "crtc.h"
#include "kernel.h"
#include "printf.h"
#include "vram.h"
#include <stdio.h>
#include <string.h>

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

void ssosmain() {
    int c = 0;
    int scancode = 0;
    uint8_t counter = 0;
    char szMessage[256];

    // init
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen
    _iocs_g_clr_on(); // clear gvram, reset palette, access page 0
    _iocs_b_curoff(); // stop cursor

    _iocs_ms_init();
    _iocs_skey_mod(0, 0, 0);
    _iocs_ms_curon();

    clear_vram_fast();
    wait_for_clear_vram_completion();

    /* _iocs_b_print("Hello\r\n"); */

    for (int i = 0; i < 16; i++) {
        fill_rect(i, 100 + 20 * i, 100 + 20 * i, 200 + 20 * i, 200 + 20 * i);
    }
#ifdef LOCAL_MODE
    print(15, 0, 0, 0, "Scott & Sandy OS x68k, [ESC] to quit");
#else
    print(15, 0, 0, 0, "Scott & Sandy OS x68k");
#endif
    for (int i = 0; i < 64; i++) {
        int fg = i % 16;
        int bg = fg ? 0 : 15;
        put_char(fg, bg, 8 * i, 200, i);
        put_char(fg, bg, 8 * i, 216, i + 64);
    }

    volatile uint8_t* mfp = (volatile uint8_t*)0xe88001;
    uint32_t prev_dt = 0;
    uint32_t prev_pos = 0;

    while (1) {
        wait_for_vsync();

        // key
        c = _iocs_b_keysns();
        if (c > 0) {
            // delete it from the buffer
            scancode = _iocs_b_keyinp();
            sprintf(szMessage, "Scan Code:        0x%08x", scancode);
            print(15, 0, 0, 100, szMessage);
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
            print(7, 0, 0, 16, szMessage);
        }

        if (pos != prev_pos) {
            prev_pos = pos;
            sprintf(szMessage, "mouse x:%3d, y:%3d", (pos & 0xFFFF0000) >> 16,
                    pos & 0x0000FFFF);
            print(8, 0, 0, 32, szMessage);
        }

        if (counter++ > 30) {
            counter = 0;
            uint32_t y = 48;

            uint32_t* timera_count = (uint32_t*)0x00300400;
            uint32_t* timerb_count = (uint32_t*)0x00300404;
            uint32_t* timerc_count = (uint32_t*)0x00300408;
            uint32_t* timerd_count = (uint32_t*)0x0030040c;
            uint32_t* key_count = (uint32_t*)0x00300410;

            sprintf(szMessage, "A: V-DISP counter: %9d (vsync count)",
                    *timera_count);
            print(9, 0, 0, y, szMessage);
            y += 16;

            // timer B is used by IOCS's key handler
            /* sprintf(szMessage, "B: timer:         %9d", *timerb_count); */
            /* print(9, 0, 0, y, szMessage); */
            /* y += 16; */

            sprintf(szMessage, "C: 100Hz timer:    %9d (every 10ms)",
                    *timerc_count);
            print(9, 0, 0, y, szMessage);
            y += 16;

            sprintf(szMessage, "D: 1000Hz timer:   %9d (every 1ms)",
                    *timerd_count);
            print(9, 0, 0, y, szMessage);
            y += 16;

            // we are using IOCS's key buffer (e.g. iocs_b_keysns/inp)
            // if you prefer your key interrupt, enable usart_handler in entry.s
            // and implement the key receive buffer
            /* sprintf(szMessage, "Key:             %9d", *key_count); */
            /* print(9, 0, 0, y, szMessage); */
            /* y += 16; */
        }
    }

    // uninit
    _iocs_ms_curof();
    _iocs_skey_mod(-1, 0, 0);
    _iocs_b_curon();  // enable cursor
    _iocs_g_clr_on(); // clear graphics, reset palette to the default, access
                      // page 0
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen
}
