#pragma once

#include <stdint.h>

static inline uint32_t _iocs_ms_getdt(void) { return 0; }
static inline uint32_t _iocs_ms_curgt(void) { return 0; }
static inline void _iocs_ms_curof(void) {}
static inline void _iocs_skey_mod(int a, int b, int c) { (void)a; (void)b; (void)c; }
static inline void _iocs_b_curon(void) {}
static inline void _iocs_g_clr_on(void) {}
static inline void _iocs_crtmod(int mode) { (void)mode; }
