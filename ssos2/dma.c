#include "gui.h"

/* --- DMAC (HD63450) Register Map (CH2 = 0xE84080) --- */
#define DMA_CH2_BASE ((volatile uint8_t*)0xE84080)

/* Registers offsets within a channel (HD63450) */
#define REG_CSR 0x00
#define REG_CER 0x01
#define REG_DCR 0x04
#define REG_OCR 0x05
#define REG_SCR 0x06
#define REG_CCR 0x07
#define REG_MTC 0x0A
#define REG_MAR 0x0C
#define REG_DAR 0x14
#define REG_BTC 0x1A  /* Base Transfer Count (chain mode) */
#define REG_BAR 0x1C  /* Base Address Register (chain mode) */
#define REG_MFC 0x29  /* Memory Function Code */
#define REG_DFC 0x31  /* Device Function Code */
#define REG_BFC 0x39  /* Base Function Code */

/* NOTE: HD63450 array chain entries are 6 bytes each: MAR(4) + MTC(2).
 * DAR is set once in the register and auto-increments via SCR.
 * Per-entry DAR is NOT supported — DAR auto-increments by MTC. */

/* X68000 HD63450 array chain entry: 6 bytes (packed) */
typedef struct __attribute__((packed)) {
    uint32_t mar;
    uint16_t mtc;
} dma_chain6_t;

#define MAX_CHAIN6_ENTRIES 512
static dma_chain6_t chain6[MAX_CHAIN6_ENTRIES];

void dma_init(void) {
    volatile uint8_t* ch = DMA_CH2_BASE;
    ch[REG_CCR] = 0x00;
    ch[REG_CSR] = 0xFF;
    ch[REG_MFC] = 0x05;  /* Supervisor data space */
    ch[REG_DFC] = 0x05;  /* Supervisor data space */
    ch[REG_BFC] = 0x05;  /* Supervisor data space (chain mode) */
}

/* Helper: Configure DMAC channel for transfer */
static void dma_config_channel(uint8_t ocr, uint8_t scr) {
    volatile uint8_t* ch = DMA_CH2_BASE;
    ch[REG_CCR] = 0x00;
    ch[REG_CSR] = 0xFF;
    ch[REG_DCR] = 0x04; /* 16-bit device port */
    ch[REG_OCR] = ocr;
    ch[REG_SCR] = scr;
}

/* Internal helper: Perform a single transfer of up to 65536 words */
static void dma_exec_once(const void* src, void* dst, uint32_t count, uint8_t ocr_val, uint8_t scr_val) {
    volatile uint8_t* ch = DMA_CH2_BASE;
    
    /* MTC=0 means 65536 in HD63450 */
    uint16_t mtc_val = (count >= 65536) ? 0 : (uint16_t)count;

    ch[REG_CCR] = 0x00; /* Stop */
    ch[REG_CSR] = 0xFF; /* Clear status */
    
    ch[REG_DCR] = 0x04; /* 16-bit device port */ /* Internal request, burst mode, 16-bit */
    ch[REG_OCR] = ocr_val;
    ch[REG_SCR] = scr_val;
    
    /* Write 16-bit MTC carefully (don't overwrite MAR) */
    *(volatile uint16_t*)&ch[REG_MTC] = mtc_val;
    *(volatile uint32_t*)&ch[REG_MAR] = (uint32_t)src;
    *(volatile uint32_t*)&ch[REG_DAR] = (uint32_t)dst;
    
    ch[REG_CCR] = 0x80; /* START */
    
    /* Wait for completion */
    while (!(ch[REG_CSR] & 0x10)) {
        if (ch[REG_CSR] & 0x02) break; /* Error check */
    }
}

void dma_transfer(const void* src, void* dst, uint32_t count, uint8_t size) {
    if (count == 0) return;
    uint8_t ocr = (size == 0) ? 0x00 : 0x00; /* MAR->DAR, 16-bit via DCR, no chain */
    
    uint32_t remaining = count;
    uint8_t* s = (uint8_t*)src;
    uint8_t* d = (uint8_t*)dst;
    uint8_t step = (size == 0) ? 1 : 2;

    while (remaining > 0) {
        uint32_t batch = (remaining > 65536) ? 65536 : remaining;
        dma_exec_once(s, d, batch, ocr, 0x05); /* MAR inc, DAR inc */
        remaining -= batch;
        s += batch * step;
        d += batch * step;
    }
}

void dma_fill(uint16_t color, void* dst, uint32_t count) {
    if (count == 0) return;
    static volatile uint16_t s_color;
    s_color = color;
    
    uint32_t remaining = count;
    uint8_t* d = (uint8_t*)dst;
    
    while (remaining > 0) {
        uint32_t batch = (remaining > 65536) ? 65536 : remaining;
        dma_exec_once((void*)&s_color, (void*)d, batch, 0x20, 0x01); /* MAR fixed, DAR inc */
        remaining -= batch;
        d += batch * 2; /* 16-bit words */
    }
}

void dma_fill_block(uint16_t color, void* dst, uint16_t w, uint16_t h, uint16_t stride) {
    if (w == 0 || h == 0) return;
    static volatile uint16_t s_color;
    s_color = color;

    volatile uint8_t* ch = DMA_CH2_BASE;

    /* For non-contiguous stride or small transfers, use per-line stop-start.
     * HD63450 array chain does NOT support per-entry DAR —
     * DAR auto-increments by MTC, so chain mode only works when stride == w. */
    if (h <= 8 || stride != w) {
        dma_config_channel(0x20, 0x01);  /* 16-bit, MAR fixed, DAR inc */

        uint8_t* d = (uint8_t*)dst;
        for (int i = 0; i < h; i++) {
            *(volatile uint16_t*)&ch[REG_MTC] = w;
            *(volatile uint32_t*)&ch[REG_MAR] = (uint32_t)&s_color;
            *(volatile uint32_t*)&ch[REG_DAR] = (uint32_t)d;
            ch[REG_CCR] = 0x80;
            while (!(ch[REG_CSR] & 0x10));
            ch[REG_CSR] = 0xFF;
            d += stride * 2;
        }
        return;
    }

    /* Array chain mode for contiguous fills (stride == w).
     * Each entry: MAR=&s_color, MTC=w. DAR auto-increments correctly. */
    int chain_count = (h > MAX_CHAIN6_ENTRIES) ? MAX_CHAIN6_ENTRIES : h;
    for (int i = 0; i < chain_count; i++) {
        chain6[i].mar = (uint32_t)&s_color;
        chain6[i].mtc = (w >= 65536) ? 0 : w;
    }

    ch[REG_CCR] = 0x00;
    ch[REG_CSR] = 0xFF;
    ch[REG_DCR] = 0x08;
    *(volatile uint32_t*)&ch[REG_BAR] = (uint32_t)chain6;
    *(volatile uint16_t*)&ch[REG_BTC] = chain_count;
    ch[REG_OCR] = 0x19; /* Array chain mode */
    ch[REG_SCR] = 0x01; /* DAR inc (MAR from chain, always &s_color) */
    *(volatile uint32_t*)&ch[REG_DAR] = (uint32_t)dst;
    ch[REG_CCR] = 0x80;

    while (!(ch[REG_CSR] & 0x90));
}

/* Contiguous word copy: RAM → VRAM (for full-screen flip)
 * Uses array chain mode (OCR=$19) when possible for maximum throughput.
 * Falls back to stop-start batches for overflow (> MAX_CHAIN6_ENTRIES * 65536). */
void dma_copy_words(const void* src, void* dst, uint32_t word_count) {
    if (word_count == 0) return;
    volatile uint8_t* ch = DMA_CH2_BASE;
    const uint8_t* s = (const uint8_t*)src;
    uint8_t* d = (uint8_t*)dst;

    ch[REG_CCR] = 0x00;
    ch[REG_CSR] = 0xFF;
    ch[REG_DCR] = 0x08; /* 16-bit device port */
    ch[REG_OCR] = 0x19; /* 16-bit, array chain, auto-request max */
    ch[REG_SCR] = 0x05; /* MAR inc, DAR inc */

    /* Build chain table: up to 65536 words per entry */
    int entries = 0;
    while (word_count > 0 && entries < MAX_CHAIN6_ENTRIES) {
        uint32_t batch = (word_count >= 65536) ? 65536 : word_count;
        chain6[entries].mar = (uint32_t)s;
        chain6[entries].mtc = (uint16_t)((batch >= 65536) ? 0 : batch);
        s += batch * 2;
        d += batch * 2;
        word_count -= batch;
        entries++;
    }

    if (entries > 0) {
        *(volatile uint32_t*)&ch[REG_BAR] = (uint32_t)chain6;
        *(volatile uint16_t*)&ch[REG_BTC] = (uint16_t)entries;
        *(volatile uint32_t*)&ch[REG_DAR] = (uint32_t)dst;
        ch[REG_CCR] = 0x80;
        while (!(ch[REG_CSR] & 0x90));
    }

    /* Fallback for overflow */
    if (word_count > 0) {
        ch[REG_CCR] = 0x00;
        ch[REG_OCR] = 0x10; /* Non-chain for fallback */
        while (word_count > 0) {
            uint16_t batch = (word_count >= 65536) ? 0 : (uint16_t)word_count;
            ch[REG_CCR] = 0x00;
            ch[REG_CSR] = 0xFF;
            *(volatile uint16_t*)&ch[REG_MTC] = batch;
            *(volatile uint32_t*)&ch[REG_MAR] = (uint32_t)s;
            *(volatile uint32_t*)&ch[REG_DAR] = (uint32_t)d;
            ch[REG_CCR] = 0x80;
            while (!(ch[REG_CSR] & 0x90));
            uint32_t transferred = (batch == 0) ? 65536 : batch;
            s += transferred * 2;
            d += transferred * 2;
            word_count -= transferred;
        }
    }
}

/* Rectangular word copy: RAM → VRAM
 * Full-width contiguous (w == stride): uses array chain mode.
 * Partial-width: per-line stop-start (DAR can't be set per chain entry). */
void dma_copy_rect_words(const void* src, void* dst, uint16_t w, uint16_t h,
                         uint16_t src_stride, uint16_t dst_stride) {
    if (w == 0 || h == 0) return;
    volatile uint8_t* ch = DMA_CH2_BASE;

    /* Chain mode: only when source and dest are contiguous (stride == w) */
    if (w == src_stride && w == dst_stride && h <= MAX_CHAIN6_ENTRIES) {
        ch[REG_CCR] = 0x00;
        ch[REG_CSR] = 0xFF;
        ch[REG_DCR] = 0x08;
        ch[REG_OCR] = 0x19; /* 16-bit, array chain, auto-request max */
        ch[REG_SCR] = 0x05;

        uint8_t* s = (uint8_t*)src;
        for (int i = 0; i < h; i++) {
            chain6[i].mar = (uint32_t)s;
            chain6[i].mtc = w;
            s += w * 2;
        }

        *(volatile uint32_t*)&ch[REG_BAR] = (uint32_t)chain6;
        *(volatile uint16_t*)&ch[REG_BTC] = h;
        *(volatile uint32_t*)&ch[REG_DAR] = (uint32_t)dst;
        ch[REG_CCR] = 0x80;
        while (!(ch[REG_CSR] & 0x90));
        return;
    }

    /* Per-line stop-start for partial-width rects */
    ch[REG_CCR] = 0x00;
    ch[REG_CSR] = 0xFF;
    ch[REG_DCR] = 0x08;
    ch[REG_OCR] = 0x10; /* Non-chain */
    ch[REG_SCR] = 0x05;

    uint8_t* s = (uint8_t*)src;
    uint8_t* d = (uint8_t*)dst;

    for (int i = 0; i < h; i++) {
        ch[REG_CCR] = 0x00;
        ch[REG_CSR] = 0xFF;
        *(volatile uint16_t*)&ch[REG_MTC] = w;
        *(volatile uint32_t*)&ch[REG_MAR] = (uint32_t)s;
        *(volatile uint32_t*)&ch[REG_DAR] = (uint32_t)d;
        ch[REG_CCR] = 0x80;
        while (!(ch[REG_CSR] & 0x90));
        s += src_stride * 2;
        d += dst_stride * 2;
    }
}

void dma_transfer_block(const void* src, void* dst, uint16_t w, uint16_t h, uint16_t src_stride, uint16_t dst_stride) {
    if (w == 0 || h == 0) return;

    /* For small transfers, use simple loop (chain setup overhead not worth it) */
    if (h <= 8) {
        volatile uint8_t* ch = DMA_CH2_BASE;
        dma_config_channel(0x20, 0x05);  /* 16-bit, MAR inc, DAR inc */

        uint8_t* s = (uint8_t*)src;
        uint8_t* d = (uint8_t*)dst;

        for (int i = 0; i < h; i++) {
            *(volatile uint16_t*)&ch[REG_MTC] = w;
            *(volatile uint32_t*)&ch[REG_MAR] = (uint32_t)s;
            *(volatile uint32_t*)&ch[REG_DAR] = (uint32_t)d;
            ch[REG_CCR] = 0x80;
            while (!(ch[REG_CSR] & 0x10));
            ch[REG_CSR] = 0xFF;
            s += src_stride * 2;
            d += dst_stride * 2;
        }
        return;
    }

    /* HD63450 array chain does NOT support per-entry DAR.
     * DAR auto-increments by MTC, so chain only works when dst_stride == w.
     * For non-contiguous dest, fall back to per-line stop-start. */
    if (dst_stride != w) {
        volatile uint8_t* ch = DMA_CH2_BASE;
        dma_config_channel(0x20, 0x05);

        uint8_t* s = (uint8_t*)src;
        uint8_t* d = (uint8_t*)dst;
        for (int i = 0; i < h; i++) {
            *(volatile uint16_t*)&ch[REG_MTC] = w;
            *(volatile uint32_t*)&ch[REG_MAR] = (uint32_t)s;
            *(volatile uint32_t*)&ch[REG_DAR] = (uint32_t)d;
            ch[REG_CCR] = 0x80;
            while (!(ch[REG_CSR] & 0x10));
            ch[REG_CSR] = 0xFF;
            s += src_stride * 2;
            d += dst_stride * 2;
        }
        return;
    }

    /* Array chain mode: dst_stride == w, DAR auto-increments correctly.
     * Each entry provides MAR (source offset per line) + MTC. */
    int chain_count = (h > MAX_CHAIN6_ENTRIES) ? MAX_CHAIN6_ENTRIES : h;
    uint8_t* s = (uint8_t*)src;
    for (int i = 0; i < chain_count; i++) {
        chain6[i].mar = (uint32_t)s;
        chain6[i].mtc = w;
        s += src_stride * 2;
    }

    volatile uint8_t* ch = DMA_CH2_BASE;
    ch[REG_CCR] = 0x00;
    ch[REG_CSR] = 0xFF;
    ch[REG_DCR] = 0x08;
    *(volatile uint32_t*)&ch[REG_BAR] = (uint32_t)chain6;
    *(volatile uint16_t*)&ch[REG_BTC] = chain_count;
    ch[REG_OCR] = 0x19; /* Array chain mode */
    ch[REG_SCR] = 0x05; /* MAR inc (from chain), DAR inc */
    *(volatile uint32_t*)&ch[REG_DAR] = (uint32_t)dst;
    ch[REG_CCR] = 0x80;

    while (!(ch[REG_CSR] & 0x90));
}
