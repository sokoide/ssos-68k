/**
 * @file quickdraw_demo.c
 * @brief QuickDraw front-buffer integration smoke test.
 */

#include "quickdraw_demo.h"

#include "quickdraw.h"
#include "quickdraw_shell.h"

static void draw_demo_palette(int16_t x, int16_t y, uint16_t swatch_width,
                              uint16_t swatch_height) {
    for (uint8_t color = 0; color < 16; color++) {
        int16_t left = (int16_t)(x + color * (swatch_width + 2));
        qd_fill_rect(left, y, swatch_width, swatch_height, color);
        qd_draw_rect(left, y, swatch_width, swatch_height, QD_COLOR_WHITE);
    }
}

static void draw_crosshair_box(int16_t x, int16_t y, uint16_t width,
                               uint16_t height, uint8_t fill_color,
                               uint8_t frame_color, uint8_t cross_color) {
    qd_fill_rect(x, y, width, height, fill_color);
    qd_draw_rect(x, y, width, height, frame_color);

    int16_t mid_x = (int16_t)(x + width / 2);
    int16_t mid_y = (int16_t)(y + height / 2);
    qd_draw_line(x, mid_y, (int16_t)(x + width - 1), mid_y, cross_color);
    qd_draw_line(mid_x, y, mid_x, (int16_t)(y + height - 1), cross_color);
}

void run_quickdraw_demo(void) {
    if (!qd_is_initialized()) {
        return;
    }

    qd_shell_draw_desktop_chrome();
    qd_shell_init_info_panel();

    draw_crosshair_box(560, 72, 176, 144, QD_COLOR_BLUE, QD_COLOR_WHITE,
                       QD_COLOR_YELLOW);

    draw_crosshair_box(560, 232, 176, 120, QD_COLOR_LTGREEN, QD_COLOR_BROWN,
                       QD_COLOR_RED);

    draw_demo_palette(560, 392, 8, 20);
}
