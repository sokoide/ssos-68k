#include "gfx.h"
#include <stdint.h>
#include <string.h>

static S3Vram vram;
static S3DmaChannel dma;

/* 8x16 font (ASCII 0x20-0x7E) - minimal set */
static const uint8_t font8x16[][16] = {
    /* Space (0x20) */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* Will use IOCS font for now */
};

void s4_dma_init(void) {
    /* HD63450 CH2 base at $E84080 */
    dma.csr = (volatile uint8_t*)(S4_DMA_CH2_BASE + 0x00);
    dma.ccr = (volatile uint8_t*)(S4_DMA_CH2_BASE + 0x07);
    dma.mar = (volatile uint32_t*)(S4_DMA_CH2_BASE + 0x0C);
    dma.dar = (volatile uint32_t*)(S4_DMA_CH2_BASE + 0x14);
    dma.mtc = (volatile uint16_t*)(S4_DMA_CH2_BASE + 0x0A);
    dma.niv = (volatile uint8_t*)(S4_DMA_CH2_BASE + 0x25);
}

void s4_dma_fill(volatile void* dst, uint16_t value, uint16_t count) {
    /* CPU-optimized fill (DMA fallback for reliability) */
    volatile uint32_t* dst32 = (volatile uint32_t*)dst;
    uint32_t val32 = ((uint32_t)value << 16) | value;
    uint16_t count32 = count / 2;

    while (count32 >= 4) {
        *dst32++ = val32;
        *dst32++ = val32;
        *dst32++ = val32;
        *dst32++ = val32;
        count32 -= 4;
    }
    while (count32 > 0) {
        *dst32++ = val32;
        count32--;
    }
}

void s4_dma_wait(void) {
    /* For CPU-based fill, this is a no-op */
}

void s4_gfx_init(void) {
    vram.draw_page = S4_GVRAM_PAGE0;
    vram.display_page = S4_GVRAM_PAGE1;
    vram.draw_idx = 0;
    vram.display_idx = 1;

    s4_dma_init();
    s4_gfx_clear(0);
}

void s4_gfx_flip(void) {
    /* Swap draw and display pages */
    volatile uint16_t* tmp = vram.draw_page;
    vram.draw_page = vram.display_page;
    vram.display_page = tmp;

    uint8_t tmp_idx = vram.draw_idx;
    vram.draw_idx = vram.display_idx;
    vram.display_idx = tmp_idx;

    /* CRTC display start address change for page flip
     * This requires writing to CRTC registers to change
     * the display start address.
     * For now, we use direct page pointer swap. */
}

void s4_gfx_clear(uint16_t color) {
    /* Clear the draw page with the specified color */
    /* In 16-color mode, each pixel is represented across 4 planes */
    /* For simplicity, fill all planes with 0 (black) */
    uint32_t page_size = S4_PAGE_SIZE;
    s4_dma_fill((volatile void*)vram.draw_page, color, (uint16_t)(page_size / 2));
}

void s4_gfx_rect(int x, int y, int w, int h, uint16_t color) {
    /* Draw a filled rectangle on the draw page
     * x,y: top-left corner (must be word-aligned for 16-color mode)
     * w,h: dimensions in pixels
     * color: 16-bit color value */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > S4_SCREEN_W) w = S4_SCREEN_W - x;
    if (y + h > S4_SCREEN_H) h = S4_SCREEN_H - y;
    if (w <= 0 || h <= 0) return;

    /* Use 32-bit writes for speed */
    uint32_t val32 = ((uint32_t)color << 16) | color;

    for (int row = y; row < y + h; row++) {
        volatile uint32_t* row_ptr = (volatile uint32_t*)
            ((uint8_t*)vram.draw_page + row * S4_BYTES_PER_LINE + (x / 16) * S4_PLANES * 2);
        int words = (w + 15) / 16;

        for (int i = 0; i < words; i++) {
            *row_ptr++ = val32; /* plane 0,1 */
            *row_ptr++ = val32; /* plane 2,3 */
        }
    }
}

void s4_gfx_hline(int x, int y, int w, uint16_t color) {
    s4_gfx_rect(x, y, w, 1, color);
}

void s4_gfx_text(int x, int y, const char* str, uint16_t fg, uint16_t bg) {
    /* Simple text rendering using IOCS font
     * For standalone/LOCAL_MODE, use IOCS B_PRINT
     * For OS mode, render directly to VRAM */
    (void)x; (void)y; (void)str; (void)fg; (void)bg;
    /* TODO: Implement bitmap font rendering for OS mode */
}
