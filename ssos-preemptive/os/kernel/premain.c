#include "kernel.h"
#include "../gfx/gfx.h"
#include <stdint.h>

extern uint8_t __bss_start, __bss_end;

void clear_bss(void) {
    uint8_t* p = &__bss_start;
    while (p < &__bss_end) {
        *p++ = 0;
    }
}

/* 16-color palette (same as standalone/main.c) */
#define PAL_RGB(r, g, b) \
    (uint16_t)((((g) & 0x1F) << 11) | (((r) & 0x1F) << 6) | \
               (((b) & 0x1F) << 1) | 1)

static void set_palette(void) {
    static const uint16_t cmap[16] = {
        PAL_RGB(0, 0, 0),       /*  0: Black */
        PAL_RGB(0, 0, 31),      /*  1: Blue */
        PAL_RGB(0, 31, 0),      /*  2: Green */
        PAL_RGB(0, 31, 31),     /*  3: Cyan */
        PAL_RGB(31, 0, 0),      /*  4: Red */
        PAL_RGB(31, 0, 31),     /*  5: Magenta */
        PAL_RGB(31, 31, 0),     /*  6: Yellow */
        PAL_RGB(31, 31, 31),    /*  7: White */
        PAL_RGB(15, 15, 15),    /*  8: Dark Gray */
        PAL_RGB(15, 15, 31),    /*  9: Light Blue */
        PAL_RGB(15, 31, 15),    /* 10: Light Green */
        PAL_RGB(15, 31, 31),    /* 11: Light Cyan */
        PAL_RGB(31, 15, 15),    /* 12: Light Red */
        PAL_RGB(31, 15, 31),    /* 13: Light Magenta */
        PAL_RGB(31, 31, 15),    /* 14: Light Yellow */
        PAL_RGB(0, 0, 0),       /* 15: Black (duplicate) */
    };
    for (int i = 0; i < 16; i++) {
        register int trapNo asm("d0") = 0x94;
        register int pal   asm("d1") = i;
        register int col   asm("d2") = cmap[i];
        asm("trap #15" :: "d"(trapNo), "d"(pal), "d"(col));
    }
}

void premain(void) {
    clear_bss();

    /* Set graphics mode via IOCS CRTMOD (D0=0x10, D1=mode) */
    {
        register int trapNo asm("d0") = 0x10;
        register int mode   asm("d1") = 16;
        asm("trap #15" :: "d"(trapNo), "d"(mode));
    }

    /* Enable graphics plane (G_CLR_ON) */
    {
        register int trapNo asm("d0") = 0x90;
        asm("trap #15" :: "d"(trapNo));
    }

    /* 
     * ROM CRTMOD(16) handler has a bug: CMP.W #$0800,D0 comparison 
     * always fails after MOVE.B load (zero-extends to word < $0800),
     * so SIZ bit (R20 bit 10) is never set for 1024x1024 virtual mode.
     * Result: virtual width defaults to 512 instead of 1024.
     * Fix: set R20 SIZ=1 manually after CRTMOD.
     */
    {
        volatile uint16_t* crtc_r20 = (volatile uint16_t*)0xE80028;
        uint16_t r20 = *crtc_r20;            /* read current */
        r20 |= 0x0400;                        /* set SIZ=1 (bit 10) */
        *crtc_r20 = r20;                      /* write back */
    }

    /* Set 16-color palette */
    set_palette();

    /* Turn off text cursor */
    {
        register int trapNo asm("d0") = 0x1F;
        asm("trap #15" :: "d"(trapNo));
    }

    /* Hide function key labels */
    {
        register int trapNo asm("d0") = 0x26;
        register int mode   asm("d1") = 0;
        asm("trap #15" :: "d"(trapNo), "d"(mode));
    }

    /* Disable soft keyboard */
    {
        register int trapNo asm("d0") = 0x7D;
        register int mode   asm("d1") = 0;
        asm("trap #15" :: "d"(trapNo), "d"(mode));
    }

    /* Clear Text VRAM to eliminate boot loader artifacts */
    {
        volatile uint32_t* tv = (volatile uint32_t*)0xE00000;
        for (int i = 0; i < 0x2000; i++) tv[i] = 0;
    }

    /* Clear CRTC scroll registers */
    {
        volatile uint16_t* crtc = (volatile uint16_t*)0xE80000;
        crtc[12] = 0;  /* Scroll X */
        crtc[13] = 0;  /* Scroll Y */
    }

    /* Initialize and show mouse cursor (may fail in IPL mode) */
    {
        register int trapNo asm("d0") = 0x70;
        asm("trap #15" :: "d"(trapNo));
    }
    {
        register int trapNo asm("d0") = 0x71;
        asm("trap #15" :: "d"(trapNo));
    }

    /* Final cursor/display cleanup before OS takes over */
    {
        register int trapNo asm("d0") = 0x1F;   /* B_CUROFF again */
        asm("trap #15" :: "d"(trapNo));
    }
    /* Full Text VRAM clear (512KB) */
    {
        volatile uint32_t* tv = (volatile uint32_t*)0xE00000;
        for (int i = 0; i < 0x20000; i++) tv[i] = 0;
    }
    /* Disable text layer in video controller to hide cursor completely */
    {
        volatile uint16_t* vc = (volatile uint16_t*)0xE82600;
        *vc = 0x003E;  /* All layers ON except text (bit0=0) */
    }

    /* Set graphics mode to default (mode 16) */
    ss_gfx_set_mode(SS_CRTMOD_16);
    ss_init();
    ss_run();
}
