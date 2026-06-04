#ifndef SS_GFX_H
#define SS_GFX_H

#include <stdint.h>

/* VRAM: 256-color mode (crtmod 8/10)
 * 1 pixel = 1 uint16_t (2 bytes)
 * Stride: 512 words (1024 bytes) per line
 * Total: 1024 lines → 1MB GVRAM
 */
#define SS_GVRAM_PAGE0  ((volatile uint16_t*)0xC00000)
#define SS_GVRAM_PAGE1  ((volatile uint16_t*)0xC80000)
#define SS_SCREEN_W     512
#define SS_SCREEN_H     512
#define SS_BYTES_PER_LINE 1024
#define SS_PAGE_SIZE    (SS_BYTES_PER_LINE * SS_SCREEN_H)

/* Font metrics */
#define SS_FONT_W   5
#define SS_FONT_H   8
#define SS_FONT_ADV 6

/* CRTC base (word-accessible) */
#define SS_CRTC_BASE   ((volatile uint16_t*)0xE80000)
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

typedef struct __attribute__((packed)) {
    uint8_t* mar;
    uint16_t mtc;
} SSXfrInf;

extern volatile uint16_t* ss_draw_page;
extern volatile uint16_t* ss_display_page;
extern uint8_t ss_draw_idx;
extern uint8_t ss_display_idx;
extern const uint8_t ss_font_data[][SS_FONT_H];

void ss_gfx_init(void);
void ss_gfx_flip(void);
void ss_gfx_clear(uint16_t color);
void ss_gfx_rect(int x, int y, int w, int h, uint16_t color);
void ss_gfx_hline(int x, int y, int w, uint16_t color);
void ss_fill_long(volatile uint32_t* dst, uint32_t val, uint32_t count);
void ss_gfx_fill_stipple(int x0, int y0, int x1, int y1, uint16_t c1, uint16_t c2);
void ss_gfx_char(int x, int y, char ch, uint16_t fg, uint16_t bg);
void ss_gfx_draw_text(int x, int y, const char* str, uint16_t fg, uint16_t bg);
void ss_gfx_char_clip(int x, int y, char ch, uint16_t fg, uint16_t bg,
                      const int* clip_wins, int nclip, int zpos);
void ss_gfx_draw_text_clip(int x, int y, const char* str, uint16_t fg, uint16_t bg,
                           const int* clip_wins, int nclip, int zpos);

void ss_dma_fill_setup(uint16_t value, int count);
int  ss_dma_fill_row(volatile uint16_t* dst, int count);

#endif /* SS_GFX_H */
