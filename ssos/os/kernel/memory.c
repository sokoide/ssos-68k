#include "memory.h"
#include <stdint.h>

const volatile uint32_t* ss_timera_counter = (volatile uint32_t*)0x150000;
const volatile uint32_t* ss_timerb_counter = (volatile uint32_t*)0x150004;
const volatile uint32_t* ss_timerc_counter = (volatile uint32_t*)0x150008;
const volatile uint32_t* ss_timerd_counter = (volatile uint32_t*)0x15000c;
const volatile uint32_t* ss_key_counter = (volatile uint32_t*)0x150010;
