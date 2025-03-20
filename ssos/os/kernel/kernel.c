#include "kernel.h"
#include <x68k/iocs.h>

const int WIDTH = 768;
const int HEIGHT = 512;
const uint16_t color_fg = 15; // foreground color
const uint16_t color_bg = 10; // background color
const uint16_t color_tb = 14; // taskbar color

volatile uint8_t* mfp = (volatile uint8_t*)0xe88001;

struct KeyBuffer ss_kb;

void ss_wait_for_vsync() {
    // if it's vsync, wait for display period
    while (!((*mfp) & 0x10))
        ;
    // wait for vsync
    while ((*mfp) & 0x10)
        ;
}

int ss_handle_keys() {
    int c;
    int scancode = 0;
    int handled_keys = 0;
    c = _iocs_b_keysns();

    while (c > 0) {
        handled_keys++;

        // delete the key from the buffer
        scancode = _iocs_b_keyinp();

        // push it to the key buffer
        // TODO: if the queue has 32 chars it's overwritten
        ss_kb.data[ss_kb.idxw] = scancode;
        ss_kb.len++;
        ss_kb.idxw++;
        if (ss_kb.idxw > 32)
            ss_kb.idxw = 0;

        if ((scancode & 0xFFFF) == 0x011b) {
            // ESC key
            return -1;
        }
        c = _iocs_b_keysns();
    }
    return handled_keys;
}
