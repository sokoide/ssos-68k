#ifndef SS_GFX_H
#define SS_GFX_H

#include <stdint.h>
#include <stddef.h>

/* Graphics Mode Constants */
#define SS_CRTMOD_8  8
#define SS_CRTMOD_16 16

/* Graphics Mode Configuration */
typedef struct {
    int crtmod;                /* IOCS crtmod value */
    int screen_w;              /* Virtual screen width */
    int screen_h;              /* Virtual screen height */
    int display_w;             /* Display width */
    int display_h;             /* Display height */
    int color_count;           /* Number of colors */
    int page_count;            /* Number of graphics pages */
    int bytes_per_line;        /* Bytes per line */
    int page_size;             /* Page size in bytes */
    volatile uint16_t* page0;  /* Page 0 VRAM address */
    volatile uint16_t* page1;  /* Page 1 VRAM address (NULL if single page) */
} SSGfxMode;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} SSGfxRect;

/* The UI task exclusively owns the mode, VRAM page state, and DMA channel 2.
 * Graphics/DMA APIs are synchronous and are not ISR-safe or reentrant. */
/* Current graphics mode (extern, defined in vram.c) */
extern const SSGfxMode* ss_current_mode;

/* Mode Selection API */
void ss_gfx_set_mode(int mode);

/* VRAM base address */
#define SS_GVRAM_BASE    ((volatile uint16_t*)0xC00000)

/* Font metrics */
#define SS_FONT_W   5
#define SS_FONT_H   8
#define SS_FONT_ADV 6

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

extern volatile uint16_t* ss_draw_page;
extern volatile uint16_t* ss_display_page;
extern uint8_t ss_draw_idx;
extern uint8_t ss_display_idx;
extern const uint8_t ss_font_data[][SS_FONT_H];

void ss_gfx_init(void);
void ss_gfx_flip(void);
void ss_gfx_clear(uint16_t color);
void ss_gfx_rect(int x, int y, int w, int h, uint16_t color);
void ss_gfx_rect_region(SSGfxRect rect, const SSGfxRect* clip, uint16_t color);
void ss_gfx_hline(int x, int y, int w, uint16_t color);
void ss_fill_long(volatile uint32_t* dst, uint32_t val, uint32_t count);
void ss_gfx_fill_stipple(int x, int y, int w, int h, uint16_t c1, uint16_t c2);
void ss_gfx_char(int x, int y, char ch, uint16_t fg, uint16_t bg);
void ss_gfx_draw_text(int x, int y, const char* str, uint16_t fg, uint16_t bg);
/* Unclipped, unrolled glyph blit. The caller MUST guarantee the glyph is
 * fully on-screen (0 <= x, x+SS_FONT_W <= display_w, same for y). Used on
 * hot text paths where the bounding rect is already known in-bounds. */
void ss_gfx_char_fast(int x, int y, char ch, uint16_t fg, uint16_t bg);
void ss_gfx_draw_text_fast(int x, int y, const char* str, uint16_t fg, uint16_t bg);
void ss_gfx_draw_text_region(int x, int y, const char* str, uint16_t fg, uint16_t bg,
                             const SSGfxRect* clip);
void ss_gfx_char_clip(int x, int y, char ch, uint16_t fg, uint16_t bg,
                      const int* clip_wins, int nclip, int zpos);
void ss_gfx_draw_text_clip(int x, int y, const char* str, uint16_t fg, uint16_t bg,
                           const int* clip_wins, int nclip, int zpos);

void ss_dma_fill_setup(uint16_t value, int count);
/* 0 = complete, -1 = DMA error after stop, -2 = timeout with stop confirmed,
 * -3 = stop unconfirmed; -3 permanently blocks graphics writes. */
int  ss_dma_fill_row(volatile uint16_t* dst, int count);
int  ss_gfx_dma_stop_unconfirmed(void);
/* Supervisor, IRQ mask held: finite stop confirmation before host cleanup. */
int  ss_gfx_dma_quiesce(void);
#if SS_PROFILE_GFX
/* Profiling only: 0 = normal threshold, 1 = CPU, 2 = DMA (eligible width). */
void ss_gfx_set_dma_bench_mode(int mode);
#endif

#ifdef SS_HOST_TEST
/* Fault injection: -1 = CPU only, 1 = abort confirmed, 2 = ACT stuck. */
void ss_gfx_test_dma_status_mode(int mode);
void ss_gfx_test_dma_in_flight(int mode);
uint16_t ss_gfx_test_dma_source_first(void);
#endif

/* XOR the perimeter of [x,y,w,h] against the draw page (clipped to screen).
 * Self-erasing: drawing the same rect twice restores the original pixels.
 * Used for transient UI (software cursor, drag outline) without a save
 * buffer; XOR itself performs a GVRAM read-modify-write. */
void ss_gfx_xor_rect(int x, int y, int w, int h);

#endif /* SS_GFX_H */
