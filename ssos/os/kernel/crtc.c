#include "crtc.h"

volatile uint16_t* crtc_reg = (uint16_t*)0xe80000;
volatile uint16_t* crtc_execution_port = (uint16_t*)0xe80480;

uint16_t get_crtc_reg(int i) { return crtc_reg[i]; }

