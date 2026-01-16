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

// Batch DMA transfer implementation
// Reduces DMA setup overhead by batching multiple spans into a single transfer

static uint16_t dma_batch_count = 0;
static uint8_t* dma_batch_dst_base = 0;

void dma_batch_begin(void) {
    dma_batch_count = 0;
    dma_batch_dst_base = 0;
    dma_prepare_x68k_16color();
}

// Add a span to the batch. Returns the span index or 0xFFFF if batch is full.
uint16_t dma_batch_add_span(uint8_t* dst, uint8_t* src, uint16_t count) {
    if (dma_batch_count >= SS_CONFIG_DMA_MAX_TRANSFERS) {
        return 0xFFFF;  // Batch full
    }

    if (dma_batch_count == 0) {
        dma_batch_dst_base = dst;
    }

    xfr_inf[dma_batch_count].addr = src;
    xfr_inf[dma_batch_count].count = count;

    return dma_batch_count++;
}

// Execute all batched DMA transfers in one operation
void dma_batch_execute(void) {
    if (dma_batch_count == 0) {
        return;
    }

    dma_clear();

    // Set up DMA for batch transfer
    dma->dar = dma_batch_dst_base;
    dma->bar = (uint8_t*)xfr_inf;
    dma->btc = dma_batch_count;

    dma_start();
    dma_wait_completion();
    dma_clear();

    dma_batch_count = 0;
}
