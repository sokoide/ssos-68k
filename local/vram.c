#include "vram.h"
#include <stdio.h>
#include <stdlib.h>

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

// globals
CRTC_REG scroll_data;

// IO ports
volatile char* mfp = (char*)0xe88001;
volatile CRTC_REG* crtc = (CRTC_REG*)0xe80018;

volatile uint16_t* vram_start = (uint16_t*)0xc00000;
volatile uint16_t* vram_end = (uint16_t*)0xd00000;

__attribute__((optimize("no-unroll-loops"))) void clear_vram() {
    /* uint16_t* p = (uint16_t*)vram0; */
    /* uint16_t* limit = p + 0x80000; */
    /*  */
    /* while (p < limit) { */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /*     *p++ = 0; */
    /* } */
}

void fill_rect(uint16_t color, int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++)
            vram_start[y * 1024 + x] = color;
    }
    return;
}

void fill_vram() {
    volatile uint16_t* p = vram_start;

    uint16_t color = 0;
    for (int y = 0; y < 512; y++) {
        for (int x = 0; x < 768 / 8; x++) {
            *p++ = color;
            *p++ = color;
            *p++ = color;
            *p++ = color;
            *p++ = color;
            *p++ = color;
            *p++ = color;
            *p++ = color;
            color++;
            if (color > 15)
                color = 0;
        }
        p += (1024 - 768);
    }
}

void put_char(uint16_t color, int x, int y, char c) {
    uint8_t* font_base = (uint8_t*)0xf3a000;
    uint16_t* font_addr = (uint16_t*)(font_base + 8 * c);


    for (int ty = y; ty < y + 8; ty++) {
        uint8_t hi = (uint8_t)((*font_addr) >> 8);
        uint8_t lo = (uint8_t)(*font_addr);
        uint8_t t = hi;

        if (ty%2 == 1) {
            t = lo;
            font_addr++;
        }

        if ((t & 0x80) != 0)
            vram_start[ty * 1024 + x] = color;
        if ((t & 0x40) != 0)
            vram_start[ty * 1024 + x + 1] = color;
        if ((t & 0x20) != 0)
            vram_start[ty * 1024 + x + 2] = color;
        if ((t & 0x10) != 0)
            vram_start[ty * 1024 + x + 3] = color;
        if ((t & 0x08) != 0)
            vram_start[ty * 1024 + x + 4] = color;
        if ((t & 0x04) != 0)
            vram_start[ty * 1024 + x + 5] = color;
        if ((t & 0x02) != 0)
            vram_start[ty * 1024 + x + 6] = color;
        if ((t & 0x01) != 0)
            vram_start[ty * 1024 + x + 7] = color;
    }
}

void init_palette() {
    _iocs_gpalet(0, 0);

    // DOS 16 color
    _iocs_gpalet(0, rgb888_2grb(0, 0, 0, 0));
    _iocs_gpalet(1, rgb888_2grb(0, 0, 170, 0));
    _iocs_gpalet(2, rgb888_2grb(0, 170, 0, 0));
    _iocs_gpalet(3, rgb888_2grb(0, 170, 170, 0));
    _iocs_gpalet(4, rgb888_2grb(170, 0, 0, 0));
    _iocs_gpalet(5, rgb888_2grb(170, 0, 170, 0));
    _iocs_gpalet(6, rgb888_2grb(170, 85, 0, 0));
    _iocs_gpalet(7, rgb888_2grb(170, 170, 170, 0));
    _iocs_gpalet(8, rgb888_2grb(85, 85, 85, 0));
    _iocs_gpalet(9, rgb888_2grb(85, 85, 255, 0));
    _iocs_gpalet(10, rgb888_2grb(85, 255, 85, 0));
    _iocs_gpalet(11, rgb888_2grb(85, 255, 255, 0));
    _iocs_gpalet(12, rgb888_2grb(255, 85, 85, 0));
    _iocs_gpalet(13, rgb888_2grb(255, 85, 255, 0));
    _iocs_gpalet(14, rgb888_2grb(255, 255, 85, 0));
    _iocs_gpalet(15, rgb888_2grb(255, 255, 255, 0));
}
