#include "memory.h"
#include <stdint.h>

const volatile uint32_t* ss_timera_counter = (volatile uint32_t*)0x030400;
const volatile uint32_t* ss_timerb_counter = (volatile uint32_t*)0x030404;
const volatile uint32_t* ss_timerc_counter = (volatile uint32_t*)0x030408;
const volatile uint32_t* ss_timerd_counter = (volatile uint32_t*)0x03040c;
const volatile uint32_t* ss_key_counter = (volatile uint32_t*)0x030410;
