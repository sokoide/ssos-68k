#ifndef S4_GFX_H
#define S4_GFX_H

#include <stdint.h>

/* VRAM layout: 768x512, 16-color mode
 * Page 0: $C00000 (draw buffer)
 * Page 1: $D00000 (display buffer, double-buffered)
 * Each scanline: 768 pixels / 8 pixels per word = 96 words per plane
 * 4 planes x 512 lines x 96 words = 196608 bytes per page
 */

#define S4_GVRAM_PAGE0  ((volatile uint16_t*)0xC00000)
#define S4_GVRAM_PAGE1  ((volatile uint16_t*)0xD00000)
#define S4_SCREEN_W     768
#define S4_SCREEN_H     512
#define S4_WORDS_PER_LINE (S4_SCREEN_W / 16)  /* 48 words per plane per line */
#define S4_PLANES       4
#define S4_BYTES_PER_LINE (S4_WORDS_PER_LINE * S4_PLANES * 2) /* 384 bytes/line */
#define S4_PAGE_SIZE    (S4_BYTES_PER_LINE * S4_SCREEN_H)     /* 196608 bytes */

/* Video controller registers */
#define S4_CRTC_BASE    0xE82000

/* Palette */
#define S4_PALETTE_BASE ((volatile uint16_t*)0xE82000)

typedef struct {
    volatile uint16_t* draw_page;
    volatile uint16_t* display_page;
    uint8_t draw_idx;
    uint8_t display_idx;
} S3Vram;

/* DMA */
#define S4_DMA_CH2_BASE  0xE84080

typedef struct {
    volatile uint8_t* csr;      /* +00 */
    volatile uint8_t* ccr;      /* +07 */
    volatile uint32_t* mar;     /* +0C */
    volatile uint32_t* dar;     /* +14 */
    volatile uint16_t* mtc;     /* +0A */
    volatile uint8_t* niv;      /* +25 */
} S3DmaChannel;

void s4_gfx_init(void);
void s4_gfx_flip(void);
void s4_gfx_clear(uint16_t color);
void s4_gfx_rect(int x, int y, int w, int h, uint16_t color);
void s4_gfx_text(int x, int y, const char* str, uint16_t fg, uint16_t bg);
void s4_gfx_hline(int x, int y, int w, uint16_t color);
void s4_dma_init(void);
void s4_dma_fill(volatile void* dst, uint16_t value, uint16_t count);
void s4_dma_wait(void);

#endif /* S4_GFX_H */
