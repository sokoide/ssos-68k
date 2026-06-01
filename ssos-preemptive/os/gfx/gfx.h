#ifndef SS_GFX_H
#define SS_GFX_H

#include <stdint.h>

/* VRAM layout: 512x512, 256-color mode
 * Page 0: $C00000 (draw buffer)
 * In 256-color mode: 1 pixel = 1 uint16_t (2 bytes)
 * Stride: 512 pixels = 512 words = 1024 bytes per line
 * Page size: 1024 bytes/line * 512 lines = 524288 bytes
 */

#define SS_GVRAM_PAGE0  ((volatile uint16_t*)0xC00000)
#define SS_SCREEN_W     512
#define SS_SCREEN_H     512
#define SS_BYTES_PER_LINE (SS_SCREEN_W * 2)  /* 1024 bytes/line */
#define SS_PAGE_SIZE    (SS_BYTES_PER_LINE * SS_SCREEN_H)  /* 524288 bytes */

typedef struct {
    volatile uint16_t* draw_page;
    volatile uint16_t* display_page;
    uint8_t draw_idx;
    uint8_t display_idx;
} SSVram;

/* DMA */
#define SS_DMA_CH2_BASE  0xE84080

typedef struct {
    volatile uint8_t* csr;      /* +00 */
    volatile uint8_t* ccr;      /* +07 */
    volatile uint32_t* mar;     /* +0C */
    volatile uint32_t* dar;     /* +14 */
    volatile uint16_t* mtc;     /* +0A */
    volatile uint8_t* niv;      /* +25 */
} SSDmaChannel;

void ss_gfx_init(void);
void ss_gfx_flip(void);
void ss_gfx_clear(uint16_t color);
void ss_gfx_rect(int x, int y, int w, int h, uint16_t color);
void ss_gfx_text(int x, int y, const char* str, uint16_t fg, uint16_t bg);
void ss_gfx_hline(int x, int y, int w, uint16_t color);
void ss_dmac_init(void);
void ss_fill(volatile void* dst, uint16_t value, uint16_t count);
void ss_dma_wait(void);

#endif /* SS_GFX_H */
