#include "vram.h"
#include "crtc.h"
#include "printf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

// globals
CRTC_REG scroll_data;

// IO ports
volatile CRTC_REG* crtc = (CRTC_REG*)0xe80018;

volatile uint16_t* vram_start = (uint16_t*)0x00c00000;
volatile uint16_t* vram_end = (uint16_t*)0x00d00000;

void ss_clear_vram() {
    // only clears 512 (height) x 1024 (width)
    volatile uint16_t* p = vram_start;
    volatile uint16_t* limit = p + 512 * 1024;

    while (p < limit) {
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
        *p++ = 0;
    }
}

void ss_clear_vram_fast() { *crtc_execution_port = (*crtc_execution_port) | 2; }

void ss_wait_for_clear_vram_completion() {
    while (*crtc_execution_port & 0b1111)
        ;
}

void ss_fill_rect(uint16_t color, int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++)
            vram_start[y * 1024 + x] = color;
    }
    return;
}

void ss_put_char(uint16_t fg_color, uint16_t bg_color, int x, int y, char c) {
    volatile uint16_t* vram = (uint16_t*)0x00C00000;

    // 8x8 font
    uint8_t* font_base_8_8 = (uint8_t*)0xf3a000;
    // 8x16 font
    uint8_t* font_base_8_16 = (uint8_t*)0xf3a800;

    uint16_t font_height = 16;
    uint16_t* font_addr = (uint16_t*)(font_base_8_16 + font_height * c);

    for (int ty = y; ty < y + font_height; ty++) {
        uint8_t hi = (uint8_t)((*font_addr) >> 8);
        uint8_t lo = (uint8_t)(*font_addr);
        uint8_t t = hi;

        if (ty % 2 == 1) {
            t = lo;
            font_addr++;
        }

        vram[ty * 1024 + x] = (t & 0x80) ? fg_color : bg_color;
        vram[ty * 1024 + x + 1] = (t & 0x40) ? fg_color : bg_color;
        vram[ty * 1024 + x + 2] = (t & 0x20) ? fg_color : bg_color;
        vram[ty * 1024 + x + 3] = (t & 0x10) ? fg_color : bg_color;
        vram[ty * 1024 + x + 4] = (t & 0x08) ? fg_color : bg_color;
        vram[ty * 1024 + x + 5] = (t & 0x04) ? fg_color : bg_color;
        vram[ty * 1024 + x + 6] = (t & 0x02) ? fg_color : bg_color;
        vram[ty * 1024 + x + 7] = (t & 0x01) ? fg_color : bg_color;
    }
}

int mystrlen(char* str) {
    int r = 0;
    while (str[r] != 0)
        r++;
    return r;
}

void ss_print(uint16_t fg_color, uint16_t bg_color, int x, int y, char* str) {
    int l = mystrlen(str);
    for (int i = 0; i < l; i++) {
        ss_put_char(fg_color, bg_color, x + i * 8, y, str[i]);
    }
}

void ss_init_palette() {
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
