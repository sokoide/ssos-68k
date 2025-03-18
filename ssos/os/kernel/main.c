#include "main.h"
#include "crtc.h"
#include "kernel.h"
#include "vram.h"
#include <stdio.h>
#include <string.h>

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

void write_vram() {
    volatile uint16_t* vram = (uint16_t*)0x00C00000;
    for (int x = 0; x < 768; x++) {
        for (int y = 0; y < 512; y++) {
            vram[x + 1024 * y] = (y / 8) % 16;
        }
    }
}

char* itoh(uint32_t value, char* buffer) {
    char* p = buffer;
    int i;

    for (i = 7; i >= 0; i--) {
        int v = (value >> (i * 4)) & 0xf;
        if (v < 10) {
            *p++ = v + '0';
        } else {
            *p++ = v - 10 + 'a';
        }
    }

    *p = '\0';
    return buffer;
}

void ssosmain() {
    char c;
    uint8_t counter = 0;
    char szMessage[256];

    /* int ssp = _iocs_b_super(0); // enter supervisor mode */

    // init
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen
    _iocs_g_clr_on(); // clear graphics, reset palette to the default, access
    // page 0
    _iocs_b_curoff(); // stop cursor
    init_palette();
    /* _iocs_ms_init(); */
    /* _iocs_skey_mod(0, 0, 0); */
    /* _iocs_ms_curon(); */

    _iocs_b_print("Hello\r\n");

    write_vram();

    /* test_vram(); */ /* uint32_t reg20 = _iocs_crtmod(255); */
    /* sprintf(szMessage, "CRTC Register 20: 0x%04X\n", reg20); */
    /* char* hex = itoh(reg20, szMessage); */
    /* _iocs_b_print("Reg20\r\n"); */
    /* _iocs_b_print(hex); */
    /* _iocs_b_print("\r\n"); */

    /* uint8_t val1 = VRAM[0]; // VRAM の最初の値を取得 */
    /* uint8_t val2 = VRAM[1]; // 2バイト目の値を取得 */
    /*  */
    /* sprintf(szMessage, "VRAM[0]=%02X, VRAM[1]=%02X\r\n", val1, val2); */
    /* _iocs_b_print(szMessage); // シリアル出力で確認 */

    /* clear_vram(); */
    /* clear_vram_fast(); */
    /* wait_for_clear_vram_completion(); */
    /* fill_vram(); */

    // for (int i = 0; i < 16; i++) {
    //     fill_rect(i, 20 + 20 * i, 20 + 20 * i, 120 + 20 * i, 120 + 20 * i);
    // }
    _iocs_b_print("Bye\r\n");

    put_char(2, 0, 200, 200, 'A');
    /* put_char(3, 0, 208, 200, 'B'); */
    /* put_char(4, 0, 216, 200, 'C'); */
    /* print(5, 0, 0, 0, "Scott & Sandy OS x68k"); */

    while (1) {
#if 0
        wait_for_vsync();

        c = _iocs_b_keysns();
        if (c) {
            break;
        }

        // do something here...
        if (counter++ > 30) {
            counter = 0;
            clear_vram();
            fill_vram();

            uint32_t dt = _iocs_ms_getdt();
            uint32_t pos = _iocs_ms_curgt();
            int8_t dx = (int8_t)(dt >> 24);
            int8_t dy = (int8_t)((dt & 0xFF0000) >> 16);
            sprintf(szMessage, "dx:%3d, dy:%3d, l:%3d, r:%3d", dx, dy,
                    (dt & 0xFF00) >> 8, dt & 0xFF);
            print(7, 0, 0, 16, szMessage);

            sprintf(szMessage, "x:%3d, y:%3d", (pos & 0xFFFF0000) >> 16,
                    pos & 0x0000FFFF);
            print(8, 0, 0, 32, szMessage);

            sprintf(szMessage, "R20: 0x%04x", get_crtc_reg(20));
            print(9, 0, 0, 48, szMessage);
        }
#endif
    }

    // uninit
    _iocs_ms_curof();
    _iocs_skey_mod(-1, 0, 0);
    _iocs_b_curon();  // enable cursor
    _iocs_g_clr_on(); // clear graphics, reset palette to the default, access
                      // page 0
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen

    /* _iocs_b_super(ssp); // leave supervisor mode */

    /* sprintf(szMessage, "Key %c pressed", c); */
    /* print(10, 0, 0, 64, szMessage); */

    while (1)
        ;
}
