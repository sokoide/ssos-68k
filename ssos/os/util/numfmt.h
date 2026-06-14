#ifndef SS_NUMFMT_H
#define SS_NUMFMT_H

#include <stdint.h>

/*
 * Lightweight integer-to-text converters used on hot rendering paths in
 * place of sprintf/snprintf.  The m68k newlib printf machinery is
 * comparatively expensive (pulls in _vfprintf + div helpers); these
 * avoid it entirely for the per-frame counter / coordinate / key-code
 * formatting.
 *
 * Every function writes a NUL-terminated string into `out` and returns
 * the number of characters written (excluding the NUL).  `out` must be
 * large enough for the longest possible representation:
 *   - dec:   up to 11 chars (uint32) / 12 chars (int32 with '-')
 *   - hex:   up to `width` chars (>=8 covers uint32)
 */

/* Unsigned decimal, e.g. 12345 -> "12345". */
int ss_utoa_dec(uint32_t v, char* out);

/* Signed decimal, e.g. -7 -> "-7". */
int ss_itoa_dec(int32_t v, char* out);

/* Signed decimal right-justified and space-padded to >= width chars
 * (matches printf "%<width>d").  If the rendered value already exceeds
 * `width`, it is written in full (no truncation). */
int ss_itoa_dec_pad(int32_t v, char* out, int width);

/* Unsigned uppercase hex, zero-padded to >= width digits
 * (matches printf "%0<width>X"). */
int ss_utoa_hex(uint32_t v, char* out, int width);

#endif /* SS_NUMFMT_H */
