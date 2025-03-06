#include "vram.h"
#include <stdio.h>

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

#define B_SUPER 0x81

int main();

int main() {
    char c;

    int ssp = _iocs_b_super(0); // enter supervisor mode

    // init
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen
    _iocs_g_clr_on(); // clear graphics, reset palette to the default, access
                      // page 0
    _iocs_b_curoff(); // stop cursor
    init_palette();

    /* fill_vram(); */
    for(int i=1;i<16;i++) {
        fill_rect(i, 20+20*i, 20+20*i, 120+20*i, 120+20*i);
    }

    while (1) {
        // if it's vsync, wait for display period
        while (!((*mfp) & 0x10))
            ;
        // wait for vsync
        while ((*mfp) & 0x10)
            ;

        c = _iocs_b_keysns();
        if (c) {
            break;
        }

        // do something here...
    }

    // uninit
    _iocs_b_curon();  // enable cursor
    _iocs_g_clr_on(); // clear graphics, reset palette to the default, access
                      // page 0
    _iocs_crtmod(16); // 768x512 dots, 16 colors, 1 screen

    _iocs_b_super(ssp);

    printf("Key %c pressed\n", c);
    return 0;
}
