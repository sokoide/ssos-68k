#include "kernel.h"
#include <x68k/iocs.h>

const int VRAMWIDTH = 1024;
const int VRAMHEIGHT = 1024;
const int WIDTH = 768;
const int HEIGHT = 512;
const uint16_t color_fg = 15; // foreground color
const uint16_t color_bg = 10; // background color
const uint16_t color_tb = 14; // taskbar color

volatile uint8_t* mfp = (volatile uint8_t*)MFP_ADDRESS;

struct KeyBuffer ss_kb;

void ss_wait_for_vsync() {
    // if it's vsync, wait for display period
    while (!((*mfp) & VSYNC_BIT))
        ;
    // wait for vsync
    while ((*mfp) & VSYNC_BIT)
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
        // if the queue is full (ss_kb.len == KEY_BUFFER_SIZE), the input key is ignored
        if (ss_kb.len < KEY_BUFFER_SIZE) {
            ss_kb.data[ss_kb.idxw] = scancode;
            ss_kb.len++;
            ss_kb.idxw++;
            if (ss_kb.idxw >= KEY_BUFFER_SIZE)
                ss_kb.idxw = 0;
        }

        if ((scancode & 0xFFFF) == ESC_SCANCODE) {
            // ESC key
            return -1;
        }
        c = _iocs_b_keysns();
    }
    return handled_keys;
}

void ss_kb_init() {
    ss_kb.idxr = 0;
    ss_kb.idxw = 0;
    ss_kb.len = 0;
}

int ss_kb_read() {
    if (ss_kb.len == 0)
        return -1;
        
    int key = ss_kb.data[ss_kb.idxr];
    ss_kb.len--;
    ss_kb.idxr++;
    if (ss_kb.idxr >= KEY_BUFFER_SIZE)
        ss_kb.idxr = 0;
        
    return key;
}

bool ss_kb_is_empty() {
    return ss_kb.len == 0;
}
