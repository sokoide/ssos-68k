#include "kernel.h"
#include <x68k/iocs.h>

volatile uint8_t* mfp = (volatile uint8_t*)0xe88001;

void wait_for_vsync() {
    // if it's vsync, wait for display period
    while (!((*mfp) & 0x10))
        ;
    // wait for vsync
    while ((*mfp) & 0x10)
        ;
}

void keyboard_interrupt_handler() {}
