#pragma once

#include <stdint.h>

extern volatile uint8_t* mfp;

void wait_for_vsync();

void keyboard_interrupt_handler();
