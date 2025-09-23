#include "quickdraw.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    uint8_t* vram_base;
    bool initialized;
    QD_Rect clip;
} QD_State;

static QD_State s_qd_state = {
    .vram_base = NULL,
    .initialized = false,
    .clip = {0, 0, QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT},
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

    uint8_t packed = (color & 0x0F);
    packed |= (packed << 4);
    memset(s_qd_state.vram_base, packed, QD_VRAM_BYTES);
}

void qd_clear_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    qd_fill_rect(x, y, width, height, color);
}

void qd_set_pixel(int16_t x, int16_t y, uint8_t color) {
    if (!qd_state_ready()) {
        return;
    }

    if (!qd_in_bounds(x, y) || !qd_clip_point(x, y)) {
        return;
    }

    size_t offset = (size_t)y * QD_BYTES_PER_ROW + ((size_t)x >> 1);
    if (offset >= QD_VRAM_BYTES) {
        return;
    }

    uint8_t* byte = s_qd_state.vram_base + offset;
    uint8_t value = color & 0x0F;

    if (x & 1) {
        *byte = (uint8_t)((*byte & 0x0F) | (value << 4));
    } else {
        *byte = (uint8_t)((*byte & 0xF0) | value);
    }
}

uint8_t qd_get_pixel(int16_t x, int16_t y) {
    if (!qd_state_ready()) {
        return 0;
    }

    if (!qd_in_bounds(x, y)) {
        return 0;
    }

    size_t offset = (size_t)y * QD_BYTES_PER_ROW + ((size_t)x >> 1);
    if (offset >= QD_VRAM_BYTES) {
        return 0;
    }

    uint8_t byte = s_qd_state.vram_base[offset];
    return (x & 1) ? (uint8_t)((byte >> 4) & 0x0F) : (uint8_t)(byte & 0x0F);
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

    int16_t rx = x;
    int16_t ry = y;
    uint16_t w = width;
    uint16_t h = height;
    if (!qd_clip_rect(&rx, &ry, &w, &h)) {
        return;
    }

    for (uint16_t row = 0; row < h; row++) {
        int16_t py = (int16_t)(ry + row);
        for (uint16_t col = 0; col < w; col++) {
            qd_set_pixel((int16_t)(rx + col), py, color);
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
