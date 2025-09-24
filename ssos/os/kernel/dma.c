#include "dma.h"
#include "ss_config.h"

XFR_INF xfr_inf[SS_CONFIG_DMA_MAX_TRANSFERS];
volatile DMA_REG* dma = (volatile DMA_REG*)0xe84080; // channel #2

void dma_init(uint8_t* dst, uint16_t block_count) {
    // Device: VRAM, incremented by 2 bytes (16-color mode: 2 bytes per pixel)
    // Source: Memory, incremented by 1 byte
    // X68000 16-color mode: VRAM is 2 bytes per pixel, lower 4 bits used
    // Transfer to lower byte of each 16-bit VRAM word
    dma->dcr = 0x00; // device (vram) 8 bit port - transfer to lower byte
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

void dma_clear() { dma->csr = 0xff; }

void dma_start() { dma->ccr |= 0x80; }

void dma_wait_completion() {
    while (!(dma->csr & 0x90))
        ;
}
