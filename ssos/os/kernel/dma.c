#include "dma.h"
#include "ss_config.h"

XFR_INF xfr_inf[SS_CONFIG_DMA_MAX_TRANSFERS];
volatile DMA_REG* dma = (volatile DMA_REG*)0xe84080; // channel #2
static int dma_x68k_prepared = 0;

void dma_init(uint8_t* dst, uint16_t block_count) {
    // Device: VRAM, incremented by 2 bytes
    // Source: Memory, incremented by 1 byte
    // Device Port Size: 16bit -> destination address incremented by 2bytes
    dma->dcr = 0x00; // device (vram) 8 bit port
    dma->ocr = 0x09; // memory->vram, 8 bit, array chaining
    dma->scr = 0x05;
    dma->ccr = 0x00;
    dma->cpr = 0x03;
    dma->mfc = 0x05;
    dma->dfc = 0x05;
    dma->bfc = 0x05;

    // dst address of vram
    dma->dar = (uint8_t*)dst;
    // 1st pointer to (src address, count)
    dma->bar = (uint8_t*)xfr_inf;
    // count of transfer blocks
    dma->btc = block_count;
}

// Prepare DMA controller for X68000 16-color VRAM transfers.
void dma_prepare_x68k_16color(void) {
    if (dma_x68k_prepared) {
        return;
    }

    dma->dcr = 0x00;  // VRAM 8-bit port - lower byte access
    dma->ocr = 0x09;  // memory->vram, 8 bit, array chaining
    dma->scr = 0x05;  // source increment by 1
    dma->ccr = 0x00;
    dma->cpr = 0x03;  // destination increment by 2 (VRAM pixel format)
    dma->mfc = 0x05;
    dma->dfc = 0x05;
    dma->bfc = 0x05;
    dma_x68k_prepared = 1;
}

// Configure a span transfer for X68000 16-color mode.
void dma_setup_span(uint8_t* dst, uint8_t* src, uint16_t count) {
    dma->dar = dst;
    dma->bar = (uint8_t*)xfr_inf;
    dma->btc = 1;

    xfr_inf[0].addr = src;
    xfr_inf[0].count = count;
}

void dma_init_x68k_16color(uint8_t* dst, uint8_t* src, uint16_t count) {
    dma_prepare_x68k_16color();
    dma_setup_span(dst, src, count);
}

void dma_clear() { dma->csr = 0xff; }

void dma_start() { dma->ccr |= 0x80; }

void dma_wait_completion() {
    while (!(dma->csr & 0x90))
        ;
}
