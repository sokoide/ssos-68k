#include "quickdraw.h"

#include "ss_config.h"
#include "kernel.h"
#include "dma.h"
#include "memory.h"

#include <stddef.h>
#include <string.h>

#ifdef TESTING
#include <stdio.h>
#endif

typedef struct {
    uint8_t* vram_base;
    bool initialized;
    QD_Rect clip;
    const uint8_t* font_base;
    uint16_t font_width;
    uint16_t font_height;
    // DMA optimization state (matching Layer system)
    uint32_t cpu_idle_time;
    uint32_t last_performance_check;
    uint16_t adaptive_dma_threshold;
} QD_State;

static QD_State s_qd_state = {
    .vram_base = NULL,
    .initialized = false,
    .clip = {0, 0, QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT},
    .font_base = NULL,
    .font_width = SS_CONFIG_FONT_WIDTH,
    .font_height = SS_CONFIG_FONT_HEIGHT,
    .cpu_idle_time = 0,
    .last_performance_check = 0,
    .adaptive_dma_threshold = QD_ADAPTIVE_DMA_THRESHOLD_DEFAULT,
};

static inline bool qd_in_bounds(int16_t x, int16_t y) {
    return (x >= 0 && x < (int16_t)QD_SCREEN_WIDTH &&
            y >= 0 && y < (int16_t)QD_SCREEN_HEIGHT);
}

static inline bool qd_state_ready(void) {
    return s_qd_state.initialized && s_qd_state.vram_base != NULL;
}

void qd_init(void) {
    s_qd_state.vram_base = (uint8_t*)0x00c00000;
    s_qd_state.clip.x = 0;
    s_qd_state.clip.y = 0;
    s_qd_state.clip.width = QD_SCREEN_WIDTH;
    s_qd_state.clip.height = QD_SCREEN_HEIGHT;
    s_qd_state.font_base = (const uint8_t*)SS_CONFIG_FONT_BASE_ADDRESS;
    s_qd_state.font_width = SS_CONFIG_FONT_WIDTH;
    s_qd_state.font_height = SS_CONFIG_FONT_HEIGHT;
    s_qd_state.initialized = true;
}

bool qd_is_initialized(void) {
    return qd_state_ready();
}

uint16_t qd_get_screen_width(void) {
    return QD_SCREEN_WIDTH;
}

uint16_t qd_get_screen_height(void) {
    return QD_SCREEN_HEIGHT;
}

void qd_set_vram_buffer(uint8_t* buffer) {
    if (buffer) {
        s_qd_state.vram_base = buffer;
    }
}

uint8_t* qd_get_vram_buffer(void) {
    return s_qd_state.vram_base;
}

void qd_set_clip_rect(int16_t x, int16_t y, uint16_t width, uint16_t height) {
    if (width == 0 || height == 0) {
        s_qd_state.clip.x = 0;
        s_qd_state.clip.y = 0;
        s_qd_state.clip.width = 0;
        s_qd_state.clip.height = 0;
        return;
    }

    int32_t x0 = x;
    int32_t y0 = y;
    int32_t x1 = x0 + width;
    int32_t y1 = y0 + height;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > QD_SCREEN_WIDTH) x1 = QD_SCREEN_WIDTH;
    if (y1 > QD_SCREEN_HEIGHT) y1 = QD_SCREEN_HEIGHT;

    if (x0 >= x1 || y0 >= y1) {
        s_qd_state.clip.x = 0;
        s_qd_state.clip.y = 0;
        s_qd_state.clip.width = 0;
        s_qd_state.clip.height = 0;
        return;
    }

    s_qd_state.clip.x = (int16_t)x0;
    s_qd_state.clip.y = (int16_t)y0;
    s_qd_state.clip.width = (uint16_t)(x1 - x0);
    s_qd_state.clip.height = (uint16_t)(y1 - y0);
}

const QD_Rect* qd_get_clip_rect(void) {
    return &s_qd_state.clip;
}

bool qd_clip_point(int16_t x, int16_t y) {
    const QD_Rect* clip = &s_qd_state.clip;
    return (x >= clip->x && x < clip->x + (int16_t)clip->width &&
            y >= clip->y && y < clip->y + (int16_t)clip->height);
}

bool qd_clip_rect(int16_t* x, int16_t* y, uint16_t* width, uint16_t* height) {
    if (!x || !y || !width || !height) {
        return false;
    }

    if (*width == 0 || *height == 0) {
        return false;
    }

    int32_t x0 = *x;
    int32_t y0 = *y;
    int32_t x1 = x0 + *width;
    int32_t y1 = y0 + *height;

    int32_t cx0 = s_qd_state.clip.x;
    int32_t cy0 = s_qd_state.clip.y;
    int32_t cx1 = cx0 + s_qd_state.clip.width;
    int32_t cy1 = cy0 + s_qd_state.clip.height;

    if (x0 < cx0) x0 = cx0;
    if (y0 < cy0) y0 = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;

    if (x0 >= x1 || y0 >= y1) {
        return false;
    }

    *x = (int16_t)x0;
    *y = (int16_t)y0;
    *width = (uint16_t)(x1 - x0);
    *height = (uint16_t)(y1 - y0);
    return true;
}

void qd_clear_screen(uint8_t color) {
    if (!qd_state_ready()) {
        return;
    }

    uint8_t value = color & 0x0F;
    uint8_t* vram = s_qd_state.vram_base;

    // X68000 16-color VRAM: 2 bytes per pixel (16-bit per pixel)
    // VRAM structure: 768 display pixels + 256 ignored pixels = 1024 pixels per row
    // Use DMA optimization for screen clearing (large memory operation)

    // Create a buffer with the pixel pattern to fill
    uint16_t pixel_value = (value << 8) | 0x00;  // 16-bit pixel value (little-endian)
    size_t pixels_per_row = QD_SCREEN_WIDTH;

    for (int y = 0; y < QD_SCREEN_HEIGHT; y++) {
        size_t row_offset = (size_t)y * QD_VRAM_STRIDE;
        uint8_t* row_start = vram + row_offset;

        // Clear only the display pixels (0-767), use DMA for large blocks
        // Use adaptive threshold to decide DMA vs CPU transfer
        size_t clear_size = pixels_per_row * 2;  // 2 bytes per pixel

        if (clear_size > s_qd_state.adaptive_dma_threshold) {
            // Use DMA for large row clearing
            uint16_t* fill_pattern = (uint16_t*)ss_mem_alloc(clear_size);
            if (fill_pattern) {
                // Fill pattern buffer with the pixel value
                for (size_t i = 0; i < pixels_per_row; i++) {
                    fill_pattern[i] = pixel_value;
                }

                dma_clear();
                dma_init(row_start, 1);
                xfr_inf[0].addr = (uint8_t*)fill_pattern;
                xfr_inf[0].count = clear_size;

                dma_start();
                dma_wait_completion();
                dma_clear();

                ss_mem_free((uint32_t)(uintptr_t)fill_pattern, clear_size);
            } else {
                // Fallback to CPU if memory allocation fails
                for (size_t x = 0; x < pixels_per_row; x++) {
                    size_t offset = x * 2;
                    row_start[offset] = 0x00;     // Low byte
                    row_start[offset + 1] = value; // High byte
                }
            }
        } else {
            // Use CPU for small blocks or when DMA fails
            for (size_t x = 0; x < pixels_per_row; x++) {
                size_t offset = x * 2;
                row_start[offset] = 0x00;     // Low byte
                row_start[offset + 1] = value; // High byte
            }
        }
    }
}

void qd_clear_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    qd_fill_rect(x, y, width, height, color);
}

void qd_set_pixel(int16_t x, int16_t y, uint8_t color) {
    if (!qd_state_ready()) {
        return;
    }

    if (!qd_in_bounds(x, y)) {
        return;
    }

    if (!qd_clip_point(x, y)) {
        // Debug: Log clipping failures for monitor panel area
        // Only in testing environment
#ifdef TESTING
        if (x >= 0 && x < 100 && y >= 0 && y < 100) {
            const QD_Rect* clip = &s_qd_state.clip;
            printf("DEBUG: qd_set_pixel clipped: (%d,%d) not in clip (%d,%d,%d,%d)\n",
                   x, y, clip->x, clip->y, clip->width, clip->height);
        }
#endif
        return;
    }

    // X68000 16-color VRAM: 2 bytes per pixel (16-bit per pixel)
    // VRAM structure: 768 display pixels + 256 ignored pixels = 1024 pixels per row
    size_t offset = (size_t)y * (SS_CONFIG_VRAM_WIDTH * 2) + (size_t)x * 2;
    if (offset >= (SS_CONFIG_VRAM_WIDTH * SS_CONFIG_VRAM_HEIGHT * 2)) {
        return;
    }

    uint8_t* pixel_bytes = s_qd_state.vram_base + offset;

    // Debug: Check if this pixel is within expected bounds for monitor panel (testing only)
#ifdef TESTING
    if (x >= 100 && x < 500 && y >= 100 && y < 400) {
        printf("DEBUG: qd_set_pixel writing to monitor panel area: (%d,%d) = %d\n", x, y, color);
    }
#endif

    // Write 16-bit pixel value (little-endian: low byte first, high byte second)
    pixel_bytes[0] = 0x00;  // Low byte (always 0 for 4-bit colors)
    pixel_bytes[1] = color & 0x0F;  // High byte (4-bit color value)

    // Debug: Verify the write
    uint8_t written_color = pixel_bytes[1];
    if (written_color != (color & 0x0F)) {
        // Write failed - this shouldn't happen
    }
}

uint8_t qd_get_pixel(int16_t x, int16_t y) {
    if (!qd_state_ready()) {
        return 0;
    }

    if (!qd_in_bounds(x, y)) {
        return 0;
    }

    // X68000 16-color VRAM: 2 bytes per pixel (16-bit per pixel)
    // VRAM structure: 768 display pixels + 256 ignored pixels = 1024 pixels per row
    size_t offset = (size_t)y * (SS_CONFIG_VRAM_WIDTH * 2) + (size_t)x * 2;
    if (offset >= (SS_CONFIG_VRAM_WIDTH * SS_CONFIG_VRAM_HEIGHT * 2)) {
        return 0;
    }

    uint8_t* pixel_bytes = s_qd_state.vram_base + offset;
    // Read 16-bit pixel value (little-endian: low byte first, high byte second)
    return (uint8_t)pixel_bytes[1];  // High byte contains the 4-bit color value
}

void qd_draw_hline(int16_t x, int16_t y, uint16_t length, uint8_t color) {
    if (length == 0 || !qd_state_ready()) {
        return;
    }

    int16_t rx = x;
    int16_t ry = y;
    uint16_t w = length;
    uint16_t h = 1;
    if (!qd_clip_rect(&rx, &ry, &w, &h)) {
        return;
    }

    for (uint16_t i = 0; i < w; i++) {
        qd_set_pixel(rx + (int16_t)i, ry, color);
    }
}

void qd_draw_vline(int16_t x, int16_t y, uint16_t length, uint8_t color) {
    if (length == 0 || !qd_state_ready()) {
        return;
    }

    int16_t rx = x;
    int16_t ry = y;
    uint16_t w = 1;
    uint16_t h = length;
    if (!qd_clip_rect(&rx, &ry, &w, &h)) {
        return;
    }

    for (uint16_t i = 0; i < h; i++) {
        qd_set_pixel(rx, ry + (int16_t)i, color);
    }
}

void qd_draw_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    if (width == 0 || height == 0 || !qd_state_ready()) {
        return;
    }

    qd_draw_hline(x, y, width, color);
    if (height > 1) {
        qd_draw_hline(x, (int16_t)(y + height - 1), width, color);
    }
    if (height > 2) {
        qd_draw_vline(x, (int16_t)(y + 1), (uint16_t)(height - 2), color);
        if (width > 1) {
            qd_draw_vline((int16_t)(x + width - 1), (int16_t)(y + 1), (uint16_t)(height - 2), color);
        }
    }
}

void qd_fill_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    if (width == 0 || height == 0 || !qd_state_ready()) {
        return;
    }

    // Debug: Log fill_rect calls for monitor panel area (testing only)
#ifdef TESTING
    if (x >= 0 && x < 100 && y >= 0 && y < 100) {
        printf("DEBUG: qd_fill_rect called with (%d,%d,%d,%d) color %d\n",
               x, y, width, height, color);
    }
#endif

    // Store original coordinates for VRAM addressing
    int16_t orig_x = x;
    int16_t orig_y = y;
    uint16_t orig_w = width;
    uint16_t orig_h = height;

    int16_t rx = x;
    int16_t ry = y;
    uint16_t w = width;
    uint16_t h = height;
    if (!qd_clip_rect(&rx, &ry, &w, &h)) {
        return;
    }

    uint8_t value = color & 0x0F;
    uint8_t* vram = s_qd_state.vram_base;

    // Use DMA optimization for large rectangle fills (matching Layer system approach)
    size_t total_pixels = (size_t)w * (size_t)h;
    size_t total_bytes = total_pixels * 2;  // 2 bytes per pixel

    // Use DMA optimization for large rectangle fills (temporarily disabled for testing)
    if (0) {
        // Use DMA for large blocks - fill row by row with DMA
        uint16_t pixel_value = (value << 8) | 0x00;  // 16-bit pixel value
        size_t row_bytes = w * 2;  // 2 bytes per pixel

        for (uint16_t row = 0; row < h; row++) {
            int16_t py = (int16_t)(ry + row);
            // VRAM offset calculation must use clipped coordinates for both X and Y
            // The clipped rectangle should be drawn at the clipped position in VRAM
            size_t row_offset = (size_t)py * QD_VRAM_STRIDE + (size_t)rx * 2;
            uint8_t* row_start = vram + row_offset;

            // Use DMA for this row if it's large enough
            if (row_bytes > s_qd_state.adaptive_dma_threshold) {
                uint16_t* fill_pattern = (uint16_t*)ss_mem_alloc(row_bytes);
                if (fill_pattern) {
                    // Fill pattern buffer with the pixel value
                    for (size_t i = 0; i < w; i++) {
                        fill_pattern[i] = pixel_value;
                    }

                    dma_clear();
                    dma_init(row_start, 1);
                    xfr_inf[0].addr = (uint8_t*)fill_pattern;
                    xfr_inf[0].count = row_bytes;

                    dma_start();
                    dma_wait_completion();
                    dma_clear();

                    ss_mem_free((uint32_t)(uintptr_t)fill_pattern, row_bytes);
                } else {
                    // Fallback to CPU if memory allocation fails
                    for (size_t col = 0; col < w; col++) {
                        size_t offset = col * 2;
                        row_start[offset] = 0x00;     // Low byte
                        row_start[offset + 1] = value; // High byte
                    }
                }
            } else {
                // Use CPU for smaller rows
                for (size_t col = 0; col < w; col++) {
                    size_t offset = col * 2;
                    row_start[offset] = 0x00;     // Low byte
                    row_start[offset + 1] = value; // High byte
                }
            }
        }
    } else {
        // Use CPU for small blocks (original implementation)
        for (uint16_t row = 0; row < h; row++) {
            int16_t py = (int16_t)(ry + row);
            for (uint16_t col = 0; col < w; col++) {
                int16_t px = (int16_t)(rx + col);
                qd_set_pixel(px, py, color);

                // Debug: Check if the pixel was written correctly for first few pixels (testing only)
#ifdef TESTING
                if (row < 2 && col < 2) {
                    uint8_t read_color = qd_get_pixel(px, py);
                    printf("DEBUG: qd_fill_rect pixel (%d,%d) = %d (expected %d)\n", px, py, read_color, color);
                }
#endif
            }
        }
    }
}

void qd_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
    if (!qd_state_ready()) {
        return;
    }

    int16_t dx = (int16_t)(x2 - x1);
    int16_t dy = (int16_t)(y2 - y1);
    int16_t sx = (dx >= 0) ? 1 : -1;
    int16_t sy = (dy >= 0) ? 1 : -1;
    dx = (int16_t)(dx >= 0 ? dx : -dx);
    dy = (int16_t)(dy >= 0 ? dy : -dy);

    int16_t err = (dx > dy ? dx : -dy) / 2;

    int16_t cx = x1;
    int16_t cy = y1;
    while (true) {
        qd_set_pixel(cx, cy, color);
        if (cx == x2 && cy == y2) {
            break;
        }
        int16_t e2 = err;
        if (e2 > -dx) {
            err = (int16_t)(err - dy);
            cx = (int16_t)(cx + sx);
        }
        if (e2 < dy) {
            err = (int16_t)(err + dx);
            cy = (int16_t)(cy + sy);
        }
    }
}

void qd_set_font_bitmap(const uint8_t* font_bitmap, uint16_t glyph_width, uint16_t glyph_height) {
    if (font_bitmap && glyph_width > 0 && glyph_height > 0) {
        s_qd_state.font_base = font_bitmap;
        s_qd_state.font_width = glyph_width;
        s_qd_state.font_height = glyph_height;
    } else {
        s_qd_state.font_base = (const uint8_t*)SS_CONFIG_FONT_BASE_ADDRESS;
        s_qd_state.font_width = SS_CONFIG_FONT_WIDTH;
        s_qd_state.font_height = SS_CONFIG_FONT_HEIGHT;
    }
}

uint16_t qd_get_font_width(void) {
    return s_qd_state.font_width;
}

uint16_t qd_get_font_height(void) {
    return s_qd_state.font_height;
}

uint16_t qd_measure_text(const char* text) {
    if (!text || s_qd_state.font_width == 0) {
        return 0;
    }

    uint16_t max_width = 0;
    uint16_t line_width = 0;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') {
            if (line_width > max_width) {
                max_width = line_width;
            }
            line_width = 0;
        } else {
            line_width = (uint16_t)(line_width + s_qd_state.font_width);
        }
    }
    if (line_width > max_width) {
        max_width = line_width;
    }
    return max_width;
}

void qd_draw_char(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color, bool opaque_bg) {
    if (!qd_state_ready()) {
        return;
    }

    if (!s_qd_state.font_base || s_qd_state.font_width == 0 || s_qd_state.font_height == 0) {
        return;
    }

    int16_t glyph_left = x;
    int16_t glyph_top = y;
    int16_t glyph_right = (int16_t)(glyph_left + (int16_t)s_qd_state.font_width);
    int16_t glyph_bottom = (int16_t)(glyph_top + (int16_t)s_qd_state.font_height);

    const QD_Rect* clip = &s_qd_state.clip;
    int16_t clip_left = clip->x;
    int16_t clip_top = clip->y;
    int16_t clip_right = (int16_t)(clip->x + (int16_t)clip->width);
    int16_t clip_bottom = (int16_t)(clip->y + (int16_t)clip->height);

    if (glyph_right <= clip_left || glyph_left >= clip_right ||
        glyph_bottom <= clip_top || glyph_top >= clip_bottom) {
        return;
    }

    int16_t draw_left = (glyph_left > clip_left) ? glyph_left : clip_left;
    int16_t draw_right = (glyph_right < clip_right) ? glyph_right : clip_right;
    int16_t draw_top = (glyph_top > clip_top) ? glyph_top : clip_top;
    int16_t draw_bottom = (glyph_bottom < clip_bottom) ? glyph_bottom : clip_bottom;

    const uint8_t* glyph = s_qd_state.font_base + ((uint16_t)(uint8_t)c * s_qd_state.font_height);

    for (int16_t py = draw_top; py < draw_bottom; ++py) {
        uint16_t row_index = (uint16_t)(py - glyph_top);
        uint8_t bits = glyph[row_index];
        int16_t start_col = (int16_t)(draw_left - glyph_left);
        int16_t end_col = (int16_t)(draw_right - glyph_left);

        for (int16_t col = start_col; col < end_col; ++col) {
            bool set = false;
            if (col >= 0 && col < (int16_t)s_qd_state.font_width) {
                int16_t shift = (int16_t)(s_qd_state.font_width - 1 - col);
                if (shift >= 0 && shift < 8) {
                    set = ((bits >> shift) & 0x01) != 0;
                }
            }

            int16_t px = (int16_t)(glyph_left + col);
            if (set) {
                qd_set_pixel(px, py, fg_color);
            } else if (opaque_bg) {
                qd_set_pixel(px, py, bg_color);
            }
        }
    }
}

void qd_draw_text(int16_t x, int16_t y, const char* text, uint8_t fg_color, uint8_t bg_color, bool opaque_bg) {
    if (!text) {
        return;
    }

    int16_t cursor_x = x;
    int16_t cursor_y = y;
    for (const char* p = text; *p; ++p) {
        if (*p == '\n') {
            cursor_x = x;
            cursor_y = (int16_t)(cursor_y + (int16_t)s_qd_state.font_height);
            continue;
        }

        qd_draw_char(cursor_x, cursor_y, *p, fg_color, bg_color, opaque_bg);
        cursor_x = (int16_t)(cursor_x + (int16_t)s_qd_state.font_width);
    }
}

// DMA optimization functions (matching Layer system)
void qd_update_performance_metrics(void) {
    // Simplified performance monitoring for QuickDraw
    // In a real system, this would track actual CPU activity
    // For now, use a simple time-based approach
    static uint32_t last_activity = 0;
    uint32_t current_time = s_qd_state.last_performance_check + 1;

    // Update performance metrics (simplified)
    uint32_t activity_delta = current_time - last_activity;

    // Adjust DMA threshold based on recent activity (matching Layer system)
    if (activity_delta < 50) {
        // High activity - prefer DMA for larger blocks
        s_qd_state.adaptive_dma_threshold = QD_ADAPTIVE_DMA_THRESHOLD_MAX;
    } else if (activity_delta > 200) {
        // Low activity - use DMA even for smaller blocks
        s_qd_state.adaptive_dma_threshold = QD_ADAPTIVE_DMA_THRESHOLD_MIN;
    } else {
        // Normal activity - use balanced threshold
        s_qd_state.adaptive_dma_threshold = QD_ADAPTIVE_DMA_THRESHOLD_DEFAULT;
    }

    last_activity = current_time;
    s_qd_state.last_performance_check = current_time;
}

void qd_memory_copy(void* dst, const void* src, size_t count) {
    if (count == 0 || !dst || !src) return;

    // Update performance metrics for adaptive behavior
    qd_update_performance_metrics();

    // ADAPTIVE DMA THRESHOLD: Use CPU load to decide transfer method
    // - High CPU activity: Use DMA for larger blocks to free up CPU
    // - Low CPU activity: Use DMA even for smaller blocks for consistency
    // - Normal activity: Balanced approach

    if (count <= s_qd_state.adaptive_dma_threshold) {
        // Use CPU transfer for small blocks (matching Layer system)
        if (count >= 4 && ((uintptr_t)src & 3) == 0 && ((uintptr_t)dst & 3) == 0) {
            // 32-bit aligned transfer for optimal CPU performance
            uint32_t* src32 = (uint32_t*)src;
            uint32_t* dst32 = (uint32_t*)dst;
            int blocks = count >> 2;
            for (int i = 0; i < blocks; i++) {
                *dst32++ = *src32++;
            }
            // Handle remaining bytes
            uint8_t* src8 = (uint8_t*)src32;
            uint8_t* dst8 = (uint8_t*)dst32;
            for (int i = 0; i < (count & 3); i++) {
                *dst8++ = *src8++;
            }
        } else {
            // Byte-by-byte transfer for unaligned data
            uint8_t* src8 = (uint8_t*)src;
            uint8_t* dst8 = (uint8_t*)dst;
            for (size_t i = 0; i < count; i++) {
                *dst8++ = *src8++;
            }
        }
        return;
    }

    // Use DMA for larger transfers (matching Layer system)
    uint8_t* dst8 = (uint8_t*)dst;
    const uint8_t* src8 = (const uint8_t*)src;

    dma_clear();
    // Configure DMA transfer
    dma_init(dst8, 1);
    xfr_inf[0].addr = (uint8_t*)src8;
    xfr_inf[0].count = count;

    // Execute DMA transfer
    dma_start();
    dma_wait_completion();
    dma_clear();
}
