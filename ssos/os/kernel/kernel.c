#include "kernel.h"
#include <x68k/iocs.h>

volatile uint8_t* mfp = (volatile uint8_t*)0xe88001;

struct KeyBuffer kb;

void wait_for_vsync() {
    // if it's vsync, wait for display period
    while (!((*mfp) & 0x10))
        ;
    // wait for vsync
    while ((*mfp) & 0x10)
        ;
}

int handle_keys() {
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
        kb.data[kb.idxw] = scancode;
        kb.len++;
        kb.idxw++;
        if (kb.idxw > 32)
            kb.idxw = 0;

#ifdef LOCAL_MODE
        if ((scancode & 0xFFFF) == 0x011b) {
            // ESC key
            return -1;
        }
#endif
        c = _iocs_b_keysns();
    }
    return handled_keys;
}
