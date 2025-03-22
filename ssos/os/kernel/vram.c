#include "vram.h"
#include "printf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

// globals
CRTC_REG scroll_data;

// IO ports
volatile CRTC_REG* crtc = (CRTC_REG*)0xe80018;
volatile uint16_t* crtc_execution_port = (uint16_t*)0xe80480;

volatile uint16_t* vram_start = (uint16_t*)0x00c00000;
volatile uint16_t* vram_end = (uint16_t*)0x00d00000;

void ss_clear_vram(volatile uint16_t* vram) {
    // only clears HEIGHT (visible height) x SW (vram width)
    volatile uint16_t* p = vram;
    volatile uint16_t* limit = p +  VRAMWIDTH * HEIGHT;

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
    ss_fill_rect_v(vram_start, VRAMWIDTH, VRAMHEIGHT, color, x0, y0, x1, y1);
}

void ss_fill_rect_v(volatile uint16_t* vram, uint16_t sw, uint16_t sh, uint16_t color, int x0, int y0, int x1, int y1){
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++)
            vram[y * sw + x] = color;
    }
    return;
}

void ss_put_char(uint16_t fg_color, uint16_t bg_color, int x, int y, char c) {
    ss_put_char_v(vram_start, VRAMWIDTH, VRAMHEIGHT, fg_color, bg_color, x, y, c);
}

void ss_put_char_v(volatile uint16_t* vram, uint16_t sw, uint16_t sh, uint16_t fg_color, uint16_t bg_color, int x, int y, char c) {
    // 8x8 font
    uint8_t* font_base_8_8 = (uint8_t*)0xf3a000;
    // 8x16 font
    uint8_t* font_base_8_16 = (uint8_t*)0xf3a800;

    uint16_t font_height = 16;
    uint16_t* font_addr = (uint16_t*)(font_base_8_16 + font_height * c);

    if (x > 1024 - 8)
        return;

    for (int ty = y; ty < y + font_height; ty++) {
        uint8_t hi = (uint8_t)((*font_addr) >> 8);
        uint8_t lo = (uint8_t)(*font_addr);
        uint8_t t = hi;

        if (ty % 2 == 1) {
            t = lo;
            font_addr++;
        }

        vram[ty * sw + x] = (t & 0x80) ? fg_color : bg_color;
        vram[ty * sw + x + 1] = (t & 0x40) ? fg_color : bg_color;
        vram[ty * sw + x + 2] = (t & 0x20) ? fg_color : bg_color;
        vram[ty * sw + x + 3] = (t & 0x10) ? fg_color : bg_color;
        vram[ty * sw + x + 4] = (t & 0x08) ? fg_color : bg_color;
        vram[ty * sw + x + 5] = (t & 0x04) ? fg_color : bg_color;
        vram[ty * sw + x + 6] = (t & 0x02) ? fg_color : bg_color;
        vram[ty * sw + x + 7] = (t & 0x01) ? fg_color : bg_color;
    }
}

int mystrlen(char* str) {
    int r = 0;
    while (str[r] != 0)
        r++;
    return r;
}

void ss_print(uint16_t fg_color, uint16_t bg_color, int x, int y, char* str) {
    ss_print_v(vram_start, VRAMWIDTH, VRAMHEIGHT, fg_color, bg_color, x, y, str);
}

void ss_print_v(volatile uint16_t* vram, uint16_t sw, uint16_t sh, uint16_t fg_color, uint16_t bg_color, int x, int y, char* str){
    int l = mystrlen(str);
    for (int i = 0; i < l; i++) {
        ss_put_char_v(vram, sw, sh, fg_color, bg_color, x + i * 8, y, str[i]);
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
