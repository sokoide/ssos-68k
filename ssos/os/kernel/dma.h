#pragma once

#include <stdint.h>
#include "ss_config.h"

// X68000環境用のNULL定義
#ifndef NULL
#define NULL ((void*)0)
#endif

typedef struct {
    uint8_t csr;
    uint8_t cer;
    uint16_t spare1;
    uint8_t dcr;
    uint8_t ocr;
    uint8_t scr;
    uint8_t ccr;
    uint16_t spare2;
    uint16_t mtc;
    uint8_t* mar;
    uint32_t spare3;
    uint8_t* dar;
    uint16_t spare4;
    uint16_t btc;
    uint8_t* bar;
    uint32_t spare5;
    uint8_t spare6;
    uint8_t niv;
    uint8_t spare7;
    uint8_t eiv;
    uint8_t spare8;
    uint8_t mfc;
    uint16_t spare9;
    uint8_t spare10;
    uint8_t cpr;
    uint16_t spare11;
    uint8_t spare12;
    uint8_t dfc;
    uint32_t spare13;
    uint16_t spare14;
    uint8_t spare15;
    uint8_t bfc;
    uint32_t spare16;
    uint8_t spare17;
    uint8_t gcr;
} DMA_REG;

typedef struct {
    uint8_t* addr;
    uint16_t count;
} XFR_INF;

extern XFR_INF xfr_inf[SS_CONFIG_DMA_MAX_TRANSFERS];
extern volatile DMA_REG* dma;

void dma_init(uint8_t* dst, uint16_t block_count);

void dma_clear();
void dma_start();
void dma_wait_completion();

// 最適化版DMA関数（フェーズ1）
void ss_dma_lazy_init();
void dma_init_optimized(uint8_t* src, uint8_t* dst, uint16_t count);
