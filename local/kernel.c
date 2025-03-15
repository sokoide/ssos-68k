#include "kernel.h"
#include <x68k/iocs.h>

volatile char* mfp = (char*)0xe88001;

void wait_for_vsync() {
    // if it's vsync, wait for display period
    while (!((*mfp) & 0x10))
        ;
    // wait for vsync
    while ((*mfp) & 0x10)
        ;
}
