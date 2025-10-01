#include "dma.h"
#include "ss_config.h"

XFR_INF xfr_inf[SS_CONFIG_DMA_MAX_TRANSFERS];
volatile DMA_REG* dma = (volatile DMA_REG*)0xe84080; // channel #2

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

// X68000 16-color mode optimized DMA transfer
void dma_init_x68k_16color(uint8_t* dst, uint8_t* src, uint16_t count) {
    // X68000 specific configuration for 16-color mode
    // Device: VRAM (lower byte only), Source: Memory
    dma->dcr = 0x00;  // VRAM 8-bit port - lower byte access
    dma->ocr = 0x09;  // memory->vram, 8 bit, array chaining
    dma->scr = 0x05;  // source increment by 1
    dma->ccr = 0x00;
    dma->cpr = 0x03;  // destination increment by 2 (VRAM pixel format)
    dma->mfc = 0x05;
    dma->dfc = 0x05;
    dma->bfc = 0x05;

    // Set up transfer for X68000 16-color mode
    dma->dar = dst;      // VRAM destination (lower byte)
    dma->bar = (uint8_t*)xfr_inf; // Transfer information pointer
    dma->btc = 1;         // Single block transfer
    
    // Configure transfer information
    xfr_inf[0].addr = src;  // Memory source
    xfr_inf[0].count = count;  // Number of pixels/bytes to transfer
}

void dma_clear() { dma->csr = 0xff; }

void dma_start() { dma->ccr |= 0x80; }

void dma_wait_completion() {
    while (!(dma->csr & 0x90))
        ;
}
