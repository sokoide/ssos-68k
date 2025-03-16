#pragma once

#include <stdint.h>

extern volatile uint16_t* crtc_reg;
extern volatile uint16_t* crtc_execution_port;

uint16_t get_crtc_reg(int i);
