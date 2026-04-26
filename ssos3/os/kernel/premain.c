#include "kernel.h"
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
    s3_init();
    s3_run();
}
