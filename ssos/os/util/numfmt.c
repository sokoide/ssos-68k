#include "numfmt.h"

/* Decimal digit table lets us avoid a per-digit divmod loop building the
 * string; we still do the divmod but keep it tight.  Output is built
 * right-to-left into a local buffer then copied. */

int ss_utoa_dec(uint32_t v, char* out) {
    char tmp[11];
    int i = (int)sizeof(tmp);
    tmp[--i] = '\0';
    if (v == 0) {
        tmp[--i] = '0';
    } else {
        while (v) {
            tmp[--i] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    int n = (int)sizeof(tmp) - 1 - i;
    for (int j = 0; j <= n; j++) out[j] = tmp[i + j];
    return n;
}

int ss_itoa_dec(int32_t v, char* out) {
    if (v < 0) {
        out[0] = '-';
        return 1 + ss_utoa_dec((uint32_t)(-(v + 1)) + 1u, out + 1);
    }
    return ss_utoa_dec((uint32_t)v, out);
}

int ss_itoa_dec_pad(int32_t v, char* out, int width) {
    char tmp[12];
    int n = ss_itoa_dec(v, tmp);          /* includes any '-' */
    int pad = width - n;
    if (pad < 0) pad = 0;
    for (int i = 0; i < pad; i++) out[i] = ' ';
    for (int i = 0; i < n; i++) out[pad + i] = tmp[i];
    out[pad + n] = '\0';
    return pad + n;
}

int ss_utoa_hex(uint32_t v, char* out, int width) {
    static const char hex[] = "0123456789ABCDEF";
    char tmp[8];
    int i = (int)sizeof(tmp);
    tmp[--i] = '\0';
    if (v == 0) {
        tmp[--i] = '0';
    } else {
        while (v) {
            tmp[--i] = hex[v & 0xF];
            v >>= 4;
        }
    }
    int digits = (int)sizeof(tmp) - 1 - i;
    int pad = width - digits;
    if (pad < 0) pad = 0;
    for (int j = 0; j < pad; j++) out[j] = '0';
    for (int j = 0; j < digits; j++) out[pad + j] = tmp[i + j];
    out[pad + digits] = '\0';
    return pad + digits;
}
