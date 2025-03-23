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
#include "ssoswindows.h"


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

    Layer* l1 = get_layer_1();
    Layer* l2 = get_layer_2();
    Layer* l3 = get_layer_3();

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

        // update_layer_3(l3);
        if (counter % 10 == 0) {
            update_layer_3(l3);
        }
        if (counter % 60 == 0) {
            update_layer_2(l2);
        }
        counter++;
        if (counter >= 240) {
            counter = 0;
        }
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
