#include "kernel.h"
#include "../gfx/gfx.h"
#include <stdint.h>

extern uint8_t __bss_start, __bss_end;

void clear_bss(void) {
    uint8_t* p = &__bss_start;
    while (p < &__bss_end) {
        *p++ = 0;
    }
}

void premain(void) {
    clear_bss();
    /* Set graphics mode to default (mode 16) */
    ss_gfx_set_mode(SS_CRTMOD_16);
    ss_init();
    ss_run();
}
