#ifndef S4_GFX_H
#define S4_GFX_H

#include <stdint.h>

/* VRAM layout: 512x512, 256-color mode
 * Page 0: $C00000 (draw buffer)
 * In 256-color mode: 1 pixel = 1 uint16_t (2 bytes)
 * Stride: 512 pixels = 512 words = 1024 bytes per line
 * Page size: 1024 bytes/line * 512 lines = 524288 bytes
 */

#define S4_GVRAM_PAGE0  ((volatile uint16_t*)0xC00000)
#define S4_SCREEN_W     512
#define S4_SCREEN_H     512
#define S4_BYTES_PER_LINE (S4_SCREEN_W * 2)  /* 1024 bytes/line */
#define S4_PAGE_SIZE    (S4_BYTES_PER_LINE * S4_SCREEN_H)  /* 524288 bytes */

typedef struct {
    volatile uint16_t* draw_page;
    volatile uint16_t* display_page;
    uint8_t draw_idx;
    uint8_t display_idx;
} S4Vram;

/* DMA */
#define S4_DMA_CH2_BASE  0xE84080

typedef struct {
    volatile uint8_t* csr;      /* +00 */
    volatile uint8_t* ccr;      /* +07 */
    volatile uint32_t* mar;     /* +0C */
    volatile uint32_t* dar;     /* +14 */
    volatile uint16_t* mtc;     /* +0A */
    volatile uint8_t* niv;      /* +25 */
} S4DmaChannel;

void s4_gfx_init(void);
void s4_gfx_flip(void);
void s4_gfx_clear(uint16_t color);
void s4_gfx_rect(int x, int y, int w, int h, uint16_t color);
void s4_gfx_text(int x, int y, const char* str, uint16_t fg, uint16_t bg);
void s4_gfx_hline(int x, int y, int w, uint16_t color);
void s4_dmac_init(void);
void s4_fill(volatile void* dst, uint16_t value, uint16_t count);
void s4_dma_wait(void);

#endif /* S4_GFX_H */
