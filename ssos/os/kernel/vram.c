#include "vram.h"
#include "kernel.h"
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
volatile uint16_t* crtc_execution_port = (uint16_t*)0xe80480;

uint16_t* vram_start = (uint16_t*)0x00c00000;
uint16_t* vram_end = (uint16_t*)0x00d00000;

void ss_clear_vram_fast() { *crtc_execution_port = (*crtc_execution_port) | 2; }

void ss_wait_for_clear_vram_completion() {
    while (*crtc_execution_port & 0b1111)
        ;
}

void ss_fill_rect_v(uint8_t* offscreen, uint16_t w, uint16_t h, uint16_t color,
                    int x0, int y0, int x1, int y1) {
    // Create 32-bit color value for faster writes (4 pixels at once)
    uint32_t color32 = color | (color << 8) | (color << 16) | (color << 24);

    for (int y = y0; y <= y1; y++) {
        uint8_t* row_start = &offscreen[y * w + x0];
        int width = x1 - x0 + 1;

        // Check if we can do aligned 32-bit writes
        if (width >= 4 && ((uintptr_t)row_start & 3) == 0) {
            uint32_t* dst32 = (uint32_t*)row_start;
            int blocks = width >> 2;  // width / 4

            // Fill 4 pixels at once using 32-bit writes
            for (int i = 0; i < blocks; i++) {
                *dst32++ = color32;
            }

            // Handle remaining pixels
            uint8_t* dst8 = (uint8_t*)dst32;
            for (int i = 0; i < (width & 3); i++) {  // width % 4
                *dst8++ = color;
            }
        } else {
            // Fallback to byte-by-byte for unaligned or small areas
            for (int x = 0; x < width; x++) {
                row_start[x] = color;
            }
        }
    }
}

void ss_draw_rect_v(uint8_t* offscreen, uint16_t w, uint16_t h, uint16_t color,
                    int x0, int y0, int x1, int y1) {
    // Draw horizontal lines (top and bottom)
    int width = x1 - x0 + 1;
    uint32_t color32 = color | (color << 8) | (color << 16) | (color << 24);

    // Top horizontal line
    uint8_t* top_line = &offscreen[y0 * w + x0];
    if (width >= 4 && ((uintptr_t)top_line & 3) == 0) {
        uint32_t* dst32 = (uint32_t*)top_line;
        int blocks = width >> 2;
        for (int i = 0; i < blocks; i++) {
            *dst32++ = color32;
        }
        uint8_t* dst8 = (uint8_t*)dst32;
        for (int i = 0; i < (width & 3); i++) {
            *dst8++ = color;
        }
    } else {
        for (int x = 0; x < width; x++) {
            top_line[x] = color;
        }
    }

    // Bottom horizontal line (if different from top)
    if (y1 != y0) {
        uint8_t* bottom_line = &offscreen[y1 * w + x0];
        if (width >= 4 && ((uintptr_t)bottom_line & 3) == 0) {
            uint32_t* dst32 = (uint32_t*)bottom_line;
            int blocks = width >> 2;
            for (int i = 0; i < blocks; i++) {
                *dst32++ = color32;
            }
            uint8_t* dst8 = (uint8_t*)dst32;
            for (int i = 0; i < (width & 3); i++) {
                *dst8++ = color;
            }
        } else {
            for (int x = 0; x < width; x++) {
                bottom_line[x] = color;
            }
        }
    }

    // Draw vertical lines (left and right)
    for (int y = y0; y <= y1; y++) {
        offscreen[y * w + x0] = color;  // Left line
        if (x1 != x0) {
            offscreen[y * w + x1] = color;  // Right line
        }
    }
}

void ss_put_char_v(uint8_t* offscreen, uint16_t w, uint16_t h,
                    uint16_t fg_color, uint16_t bg_color, int x, int y, char c) {
    // 8x16 font - OPTIMIZED: batch 8-pixel writes
    uint8_t* font_base_8_16 = (uint8_t*)0xf3a800;
    uint16_t font_height = 16;
    uint8_t* font_addr = (uint8_t*)(font_base_8_16 + font_height * c);

    if (x > w - 8)
        return;

    // Pre-compute color lookup table for faster bit->color conversion
    uint8_t colors[2] = { (uint8_t)bg_color, (uint8_t)fg_color };

    // Process each font row
    for (int ty = 0; ty < font_height; ty++) {
        uint8_t font_byte = font_addr[ty];  // Get single byte for this row
        uint8_t* dst = &offscreen[(y + ty) * w + x];

        // Optimized: unroll 8 pixels with lookup table (no conditional per pixel)
        dst[0] = colors[(font_byte >> 7) & 1];
        dst[1] = colors[(font_byte >> 6) & 1];
        dst[2] = colors[(font_byte >> 5) & 1];
        dst[3] = colors[(font_byte >> 4) & 1];
        dst[4] = colors[(font_byte >> 3) & 1];
        dst[5] = colors[(font_byte >> 2) & 1];
        dst[6] = colors[(font_byte >> 1) & 1];
        dst[7] = colors[font_byte & 1];
    }
}

int mystrlen(char* str) {
    int r = 0;
    while (str[r] != 0)
        r++;
    return r;
}

void ss_print_v(uint8_t* offscreen, uint16_t w, uint16_t h, uint16_t fg_color,
                uint16_t bg_color, int x, int y, char* str) {
    int l = mystrlen(str);
    for (int i = 0; i < l; i++) {
        ss_put_char_v(offscreen, w, h, fg_color, bg_color, x + i * 8, y,
                      str[i]);
    }
}

// Smart text printing - only draws if text has changed
// Returns 1 if text was redrawn, 0 if no change
int ss_print_v_smart(uint8_t* offscreen, uint16_t w, uint16_t h, uint16_t fg_color,
                     uint16_t bg_color, int x, int y, char* str, char* prev_str) {
    // Quick string comparison - if strings are identical, don't redraw
    if (prev_str != NULL) {
        int i = 0;
        while (str[i] != 0 && prev_str[i] != 0) {
            if (str[i] != prev_str[i]) break;
            i++;
        }
        // If strings are identical, skip drawing
        if (str[i] == 0 && prev_str[i] == 0) {
            return 0; // No change
        }
    }

    // Clear the old text area first (fill with background color)
    int old_len = (prev_str != NULL) ? mystrlen(prev_str) : 0;
    int new_len = mystrlen(str);
    int max_len = (old_len > new_len) ? old_len : new_len;

    // Clear the maximum text area
    ss_fill_rect_v(offscreen, w, h, bg_color, x, y, x + max_len * 8 - 1, y + 15);

    // Draw the new text
    ss_print_v(offscreen, w, h, fg_color, bg_color, x, y, str);

    return 1; // Text was redrawn
}

// PERFORMANCE OPTIMIZATION: Enhanced memory operations with cache alignment
void ss_memcpy_32(uint32_t* dst, const uint32_t* src, size_t count) {
    // Handle unaligned start (up to 3 bytes)
    while (count > 0 && ((uintptr_t)dst & 3)) {
        *dst++ = *src++;
        count--;
    }

    // Main 32-bit aligned copy loop - unrolled for better performance
    while (count >= 4) {
        // Unroll 4 iterations for better instruction-level parallelism
        *dst++ = *src++;
        *dst++ = *src++;
        *dst++ = *src++;
        *dst++ = *src++;
        count -= 4;
    }

    // Handle remaining bytes
    while (count > 0) {
        *dst++ = *src++;
        count--;
    }
}

// Fast memory set using 32-bit transfers with alignment optimization
void ss_memset_32(uint32_t* dst, uint32_t value, size_t count) {
    // Handle unaligned start
    while (count > 0 && ((uintptr_t)dst & 3)) {
        *dst++ = value;
        count--;
    }

    // Main aligned set loop - unrolled
    while (count >= 4) {
        *dst++ = value;
        *dst++ = value;
        *dst++ = value;
        *dst++ = value;
        count -= 4;
    }

    // Handle remaining bytes
    while (count > 0) {
        *dst++ = value;
        count--;
    }
}

// PERFORMANCE OPTIMIZATION: Cache-friendly memory fill for rectangles
void ss_fill_rect_v_fast_aligned(uint8_t* offscreen, uint16_t w, uint16_t h,
                                 uint16_t color, int x0, int y0, int x1, int y1) {
    // Enhanced version with cache alignment considerations
    uint32_t color32 = color | (color << 8) | (color << 16) | (color << 24);

    for (int y = y0; y <= y1; y++) {
        uint8_t* row_start = &offscreen[y * w + x0];
        int width = x1 - x0 + 1;

        // Handle unaligned start bytes
        while (width > 0 && ((uintptr_t)row_start & 3)) {
            *row_start++ = color;
            width--;
        }

        // Main aligned 32-bit fill loop
        if (width >= 4) {
            uint32_t* dst32 = (uint32_t*)row_start;
            int blocks = width >> 2;

            // Unrolled loop for better performance
            for (int i = 0; i < blocks; i += 4) {
                if (i + 4 <= blocks) {
                    // Process 4 blocks at once (16 bytes)
                    dst32[0] = color32;
                    dst32[1] = color32;
                    dst32[2] = color32;
                    dst32[3] = color32;
                    dst32 += 4;
                } else {
                    // Handle remaining blocks
                    for (int j = i; j < blocks; j++) {
                        *dst32++ = color32;
                    }
                    break;
                }
            }

            // Update pointers and remaining width
            row_start = (uint8_t*)dst32;
            width = width & 3;  // width % 4
        }

        // Handle remaining unaligned bytes
        while (width > 0) {
            *row_start++ = color;
            width--;
        }
    }
}

// Optimized rectangle fill - alias to the existing optimized function
void ss_fill_rect_v_fast(uint8_t* offscreen, uint16_t w, uint16_t h,
                         uint16_t color, int x0, int y0, int x1, int y1) {
    ss_fill_rect_v(offscreen, w, h, color, x0, y0, x1, y1);
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
