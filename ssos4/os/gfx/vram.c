#include "gfx.h"
#include <stdint.h>
#include <string.h>

static S4Vram vram;
static S4DmaChannel dma;

void s4_dmac_init(void) {
    /* HD63450 CH2 base at $E84080 */
    dma.csr = (volatile uint8_t*)(S4_DMA_CH2_BASE + 0x00);
    dma.ccr = (volatile uint8_t*)(S4_DMA_CH2_BASE + 0x07);
    dma.mar = (volatile uint32_t*)(S4_DMA_CH2_BASE + 0x0C);
    dma.dar = (volatile uint32_t*)(S4_DMA_CH2_BASE + 0x14);
    dma.mtc = (volatile uint16_t*)(S4_DMA_CH2_BASE + 0x0A);
    dma.niv = (volatile uint8_t*)(S4_DMA_CH2_BASE + 0x25);
}

void s4_fill(volatile void* dst, uint16_t value, uint16_t count) {
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
    vram.display_page = S4_GVRAM_PAGE0;
    vram.draw_idx = 0;
    vram.display_idx = 0;

    s4_dmac_init();
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
    /* In 256-color mode, each pixel is 1 uint16_t */
    uint32_t page_size = S4_PAGE_SIZE;
    s4_fill((volatile void*)vram.draw_page, color, (uint16_t)(page_size / 2));
}

void s4_gfx_rect(int x, int y, int w, int h, uint16_t color) {
    /* Draw a filled rectangle on the draw page
     * x,y: top-left corner
     * w,h: dimensions in pixels
     * color: 16-bit color value (256-color mode: 1 pixel = 1 uint16_t) */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > S4_SCREEN_W) w = S4_SCREEN_W - x;
    if (y + h > S4_SCREEN_H) h = S4_SCREEN_H - y;
    if (w <= 0 || h <= 0) return;

    /* Use 32-bit writes for speed */
    uint32_t val32 = ((uint32_t)color << 16) | color;

    for (int row = y; row < y + h; row++) {
        volatile uint32_t* row_ptr = (volatile uint32_t*)
            ((uint8_t*)vram.draw_page + row * S4_BYTES_PER_LINE + x * 2);

        for (int i = 0; i < w / 2; i++) {
            *row_ptr++ = val32;
        }

        /* Handle odd width */
        if (w % 2 == 1) {
            volatile uint16_t* ptr16 = (volatile uint16_t*)row_ptr;
            *ptr16 = color;
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
