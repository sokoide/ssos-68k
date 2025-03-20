#pragma once

/* 0x000000-0x001FFF: Interrupt vector, IOCS work or etc (8KiB)
 * 0x002000-0x0023FF: Boot sector (1KiB)
 * 0x0023FF-0x00FFFF: SSOS stack (55KiB)
 * 0x010000-0x02FFFF: SSOS .text (128KiB)
 * 0x030000-0x03FFFF: SSOS .data (64KiB)
 * 0x100000-0xBFFFFF: SSOS application memory (11MiB)
 *
 * 1 chunk = 4KiB
 * 11MiB / 4Kib = 2816 chunks
 */

