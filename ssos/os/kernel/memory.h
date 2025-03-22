#pragma once
#include <stdint.h>

/* common)
 * 0x000000-0x001FFF: Interrupt vector, IOCS work or etc (8KiB)
 *
 * disk boot mode)
 * 0x002000-0x0023FF: Boot sector (1KiB)
 * 0x0023FF-0x00FFFF: SSOS supervisor mode stack (55KiB)
 * 0x010000-0x02FFFF: SSOS .text (128KiB)
 * 0x030000-0x03FFFF: SSOS .data (64KiB)
 * 0x150000-0x15FFFF: SSOS .ssos (64KiB) - not used yet
 * 0x160000-0xBFFFFF: SSOS .app (10.7MiB), application memory
 *
 * local mode)
 * supervisor stack, .text, .data, .bss auto assigned
 * app memory allocated by malloc()
 *
 * 1 memory chunk = 4KiB
 * 11MiB / 4Kib = 2816 chunks
 */

