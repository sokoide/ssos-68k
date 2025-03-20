#pragma once
#include <stdint.h>

/* 0x000000-0x001FFF: Interrupt vector, IOCS work or etc (8KiB)
 * 0x002000-0x0023FF: Boot sector (1KiB)
 *
 * disk boot mode)
 * 0x0023FF-0x00FFFF: SSOS stack (55KiB)
 * 0x010000-0x02FFFF: SSOS .text (128KiB)
 * 0x030000-0x03FFFF: SSOS .data (64KiB)
 * 0x040000-0x0FFFFF: SSOS .ssos (768KiB),
 * 0x100000-0xBFFFFF: SSOS .app (11MiB), application memory
 *
 * local mode)
 * TBD
 *
 * 1 chunk = 4KiB
 * 11MiB / 4Kib = 2816 chunks
 */

// SSOS ports
extern const volatile uint32_t* ss_timera_counter;
extern const volatile uint32_t* ss_timerb_counter;
extern const volatile uint32_t* ss_timerc_counter;
extern const volatile uint32_t* ss_timerd_counter;
extern const volatile uint32_t* ss_key_counter;
