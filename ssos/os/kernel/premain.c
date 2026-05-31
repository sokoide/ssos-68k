#include "ssosmain.h"
#include <stdint.h>

extern uint8_t __bss_start, __bss_end;
extern uint8_t __data_rom_start, __data_start, __data_end;

void clear_bss() {
    // BSS is only cleared once at startup; byte loop is fine
    uint8_t* p = &__bss_start;
    while (p < &__bss_end) {
        *p++ = 0;
    }
}

void copy_data() {
    // This is not necessary
    // .data section is loaded along with the .text section in boot/main.s
}

void premain() {
    clear_bss();
    copy_data();
    ssosmain();
}
