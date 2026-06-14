#include "../kernel.h"
#include "../../gfx/gfx.h"
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
     * ROM CRTMOD(16) handler misses SIZ bit. Set CRTC R20 bit10=1.
     */
    {
        volatile uint16_t* crtc_r20 = (volatile uint16_t*)0xE80028;
        uint16_t r20 = *crtc_r20;
        r20 |= 0x0400;
        *crtc_r20 = r20;
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
    /* NOTE: MS_INIT ($70) and MS_CURON ($71) are now called from boot.s
     * (before OS entry) and re-called AFTER ss_set_interrupts() below
     * (after MFP is set to Human68K-compatible $FF/$7F). The earlier
     * call here is removed because it happened BEFORE the MFP was
     * configured to $FF/$7F, which could leave the SCC mouse receiver
     * in an inconsistent state.
     */

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

    /*
     * Re-initialize MFP interrupts AFTER all IOCS calls above.
     *
     * entry.s calls ss_set_interrupts() BEFORE premain(), but the IOCS
     * routines invoked above (CRTMOD, G_CLR_ON, etc.) internally
     * reprogram the MFP — timers, IMR, and the interrupt vectors —
     * clobbering our V-DISP (0x134) and Timer D (0x110) handlers.  With
     * those gone, ss_vsync_counter never advances, so ss_run()'s first
     * wait_vsync() freezes: the screen shows only the initial render
     * (background + empty windows) and never responds to keyboard/mouse.
     *
     * Mirror standalone/main.c, which calls ss_set_interrupts() LAST for
     * exactly this reason.  This re-applies IMR=$FF/$7F and reinstalls
     * our V-DISP / Timer D handlers so the scheduler and input wake up.
     */
    ss_set_interrupts();

    /*
     * Re-issue _MS_INIT / _MS_CURON AFTER ss_set_interrupts() so the
     * IPL-ROM mouse handler is installed in a clean MFP=$FF/$7F state.
     * boot.s already called them before OS entry; this second call
     * guarantees the SCC Ch.B receiver is fully active regardless of
     * whatever state the prior IOCS calls (CRTMOD, G_CLR_ON) left it in.
     * Without this, the IPL-ROM _MS_* work area may be initialized but
     * _MS_CURGT returns the stale value 00BC00FE because no SCC
     * receive interrupt has been routed to a vector.
     */
    {
        register int trapNo asm("d0") = 0x70;
        asm("trap #15" :: "d"(trapNo));
    }
    {
        register int trapNo asm("d0") = 0x71;
        asm("trap #15" :: "d"(trapNo));
    }

    /*
     * baremetal 固有の MFP 調整 (AER のみ。IMR は Human68K 互換 $FF/$7F
     * を維持する)。
     *
     * AER=$06: Timer A をイベントカウントモードで V-DISP 計測に使うため、
     * GPIP4 のエッジ方向を Human68K 互換 (V-DISP の 1→0 変化でカウント)
     * に明示設定。ss_set_interrupts() は AER を書き換えないため IPL ROM
     * の値に依存するが、これが不適切だと Timer A がカウントせず
     * ss_vsync_counter が進まない (wait_vsync でフリーズ)。
     * (x68k-master/04_MFP.md §5「イベントカウントモード」)
     *
     * IMRA=$FF / IMRB=$7F を維持する理由: SCC (IPL レベル5) と
     * MFP 内部ソースをすべて許可する Human68K 互換設定。
     * 以前は IMRA=$21/IMRB=$10 で未使用ソース発火を避けていたが、
     * その絞り込みが IPL ROM ハンドラが期待する状態を崩し、
     * 結果としてマウス受信ワークが更新されない症状の原因になっていた。
     */
    *(volatile uint8_t*)0xE88003 = 0x06;   /* AER */
    /* IMRA/IMRB are left at the $FF/$7F set by ss_set_interrupts(). */

    /* Set graphics mode to default (mode 16) */
    ss_gfx_set_mode(SS_CRTMOD_16);
    ss_init();
    ss_run();
}
