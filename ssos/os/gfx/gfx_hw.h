#ifndef SS_GFX_HW_H
#define SS_GFX_HW_H

#include <stdint.h>
#include <stddef.h>

/* X68000 graphics implementation and MMIO-aware tests only. */
/* VRAM base address */
#define SS_GVRAM_BASE    ((volatile uint16_t*)0xC00000)

/* CRTC base (word-accessible). Native pixel tests replace this MMIO mapping
 * with RAM; target builds always retain the physical X68000 address. */
#ifdef SS_HOST_TEST
extern volatile uint16_t ss_gfx_test_crtc[];
#define SS_CRTC_BASE   ss_gfx_test_crtc
#else
#define SS_CRTC_BASE   ((volatile uint16_t*)0xE80000)
#endif
#define SS_CRTC_SCROLL_Y 13

/* DMAC CH2 */
#define SS_DMA_CH2_BASE  0xE84080
#define SS_DMA_FILL_THRESHOLD 64

typedef struct __attribute__((packed)) {
    uint8_t  csr;
    uint8_t  cer;
    uint16_t spare1;
    uint8_t  dcr;
    uint8_t  ocr;
    uint8_t  scr;
    uint8_t  ccr;
    uint16_t spare2;
    uint16_t mtc;
    uint8_t* mar;
    uint32_t spare3;
    uint8_t* dar;
    uint16_t spare4;
    uint16_t btc;
    uint8_t* bar;
    uint32_t spare5;
    uint8_t  spare6;
    uint8_t  niv;
    uint8_t  spare7;
    uint8_t  eiv;
    uint8_t  spare8;
    uint8_t  mfc;
    uint16_t spare9;
    uint8_t  spare10;
    uint8_t  cpr;
    uint16_t spare11;
    uint8_t  spare12;
    uint8_t  dfc;
    uint32_t spare13;
    uint16_t spare14;
    uint8_t  spare15;
    uint8_t  bfc;
} SSDmaReg;

/* HD63450 channel registers are laid out with byte/word/long fields at
 * fixed offsets.  The packed pointers must remain 32-bit on the target.
 * Native builds use host-width pointers and cannot validate this ABI. */
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(SSDmaReg, csr) == 0, "DMAC CSR offset changed");
_Static_assert(offsetof(SSDmaReg, dcr) == 4, "DMAC DCR offset changed");
_Static_assert(offsetof(SSDmaReg, ccr) == 7, "DMAC CCR offset changed");
_Static_assert(offsetof(SSDmaReg, mtc) == 10, "DMAC MTC offset changed");
_Static_assert(offsetof(SSDmaReg, mar) == 12, "DMAC MAR offset changed");
_Static_assert(offsetof(SSDmaReg, dar) == 20, "DMAC DAR offset changed");
_Static_assert(offsetof(SSDmaReg, btc) == 26, "DMAC BTC offset changed");
_Static_assert(offsetof(SSDmaReg, bar) == 28, "DMAC BAR offset changed");
_Static_assert(offsetof(SSDmaReg, mfc) == 41, "DMAC MFC offset changed");
_Static_assert(offsetof(SSDmaReg, dfc) == 49, "DMAC DFC offset changed");
_Static_assert(sizeof(SSDmaReg) == 58, "DMAC channel register layout changed");
#endif

typedef struct __attribute__((packed)) {
    uint8_t* mar;
    uint16_t mtc;
} SSXfrInf;

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(SSXfrInf, mar) == 0, "DMA descriptor MAR offset changed");
_Static_assert(offsetof(SSXfrInf, mtc) == 4, "DMA descriptor MTC offset changed");
_Static_assert(sizeof(SSXfrInf) == 6, "DMA descriptor must be 6 bytes");
#endif

#endif /* SS_GFX_HW_H */
