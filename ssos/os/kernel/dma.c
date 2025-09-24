#include "dma.h"
#include "ss_config.h"
#include "ss_perf.h"

XFR_INF xfr_inf[SS_CONFIG_DMA_MAX_TRANSFERS];
volatile DMA_REG* dma = (volatile DMA_REG*)0xe84080; // channel #2

// DMAレジスタの事前設定とLazy初期化（最適化版）
static volatile DMA_REG* dma_cached = NULL;
static uint32_t dma_init_count = 0;

void ss_dma_lazy_init() {
    if (!dma_cached) {
        dma_cached = (volatile DMA_REG*)0xe84080;
        // 共通設定を事前設定（Layer描画用）
        dma_cached->dcr = 0x00;  // VRAM 8-bit port
        dma_cached->ocr = 0x09;  // memory->vram, 8 bit, array chaining
        dma_cached->scr = 0x05;
        dma_cached->ccr = 0x00;
        dma_cached->cpr = 0x03;
        dma_cached->mfc = 0x05;
        dma_cached->dfc = 0x05;
        dma_cached->bfc = 0x05;
    }
}

// 最適化版DMA初期化関数
void dma_init_optimized(uint8_t* src, uint8_t* dst, uint16_t count) {
    SS_PERF_START_MEASUREMENT(SS_PERF_DMA_INIT);

    ss_dma_lazy_init();
    dma_clear();

    // 変更されたレジスタのみ設定（高速化）
    dma_cached->dar = dst;
    dma_cached->bar = (uint8_t*)&xfr_inf[0];
    dma_cached->btc = 1;

    xfr_inf[0].addr = src;
    xfr_inf[0].count = count;

    dma_start();
    dma_wait_completion();
    dma_clear();

    dma_init_count++;
    SS_PERF_END_MEASUREMENT(SS_PERF_DMA_INIT);
}

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
