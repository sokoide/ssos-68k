#include "../framework/ssos_test.h"

#include <string.h>

#include "../../os/window/quickdraw.h"

static uint8_t test_vram[QD_VRAM_BYTES] __attribute__((aligned(4)));
static uint8_t test_font[256 * 16];

static void reset_vram_buffer(void) {
    memset(test_vram, 0, sizeof(test_vram));
}

static void setup_quickdraw_system(void) {
    reset_vram_buffer();
    qd_init();
    qd_set_vram_buffer(test_vram);
    qd_clear_screen(QD_COLOR_BLACK);
}

static void setup_font_stub(void) {
    memset(test_font, 0, sizeof(test_font));

    for (int row = 0; row < 16; ++row) {
        test_font[((size_t)'A' * 16) + row] = (row == 0 || row == 15) ? 0x7E : 0x81;
        test_font[((size_t)'B' * 16) + row] = (row == 0 || row == 7 || row == 15) ? 0xFE : 0x81;
    }

    qd_set_font_bitmap(test_font, 8, 16);
}

TEST(quickdraw_initialization_basic) {
    reset_vram_buffer();
    qd_init();
    ASSERT_TRUE(qd_is_initialized());
    ASSERT_EQ(qd_get_screen_width(), QD_SCREEN_WIDTH);
    ASSERT_EQ(qd_get_screen_height(), QD_SCREEN_HEIGHT);

    qd_set_vram_buffer(test_vram);
    ASSERT_EQ(qd_get_vram_buffer(), test_vram);

    const QD_Rect* clip = qd_get_clip_rect();
    ASSERT_EQ(clip->x, 0);
    ASSERT_EQ(clip->y, 0);
    ASSERT_EQ(clip->width, QD_SCREEN_WIDTH);
    ASSERT_EQ(clip->height, QD_SCREEN_HEIGHT);
}

TEST(quickdraw_pixel_operations) {
    setup_quickdraw_system();

    qd_set_pixel(100, 100, QD_COLOR_WHITE);

    size_t offset = (100 * QD_SCREEN_WIDTH + 100) / 2;
    uint8_t* vram = qd_get_vram_buffer();
    ASSERT_EQ(vram[offset] & 0x0F, QD_COLOR_WHITE & 0x0F);

    ASSERT_EQ(qd_get_pixel(100, 100), QD_COLOR_WHITE);
    qd_set_pixel(0, 0, QD_COLOR_RED);
    ASSERT_EQ(qd_get_pixel(0, 0), QD_COLOR_RED);
    qd_set_pixel(QD_SCREEN_WIDTH - 1, QD_SCREEN_HEIGHT - 1, QD_COLOR_BLUE);
    ASSERT_EQ(qd_get_pixel(QD_SCREEN_WIDTH - 1, QD_SCREEN_HEIGHT - 1), QD_COLOR_BLUE);
    ASSERT_EQ(qd_get_pixel(QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT), QD_COLOR_BLACK);
}

TEST(quickdraw_rectangle_operations) {
    setup_quickdraw_system();

    qd_draw_rect(50, 50, 100, 80, QD_COLOR_GREEN);

    ASSERT_EQ(qd_get_pixel(50, 50), QD_COLOR_GREEN);
    ASSERT_EQ(qd_get_pixel(149, 50), QD_COLOR_GREEN);
    ASSERT_EQ(qd_get_pixel(50, 129), QD_COLOR_GREEN);
    ASSERT_EQ(qd_get_pixel(149, 129), QD_COLOR_GREEN);
    ASSERT_EQ(qd_get_pixel(100, 100), QD_COLOR_BLACK);
}

TEST(quickdraw_fill_rect_operations) {
    setup_quickdraw_system();

    qd_fill_rect(25, 25, 50, 40, QD_COLOR_YELLOW);

    ASSERT_EQ(qd_get_pixel(25, 25), QD_COLOR_YELLOW);
    ASSERT_EQ(qd_get_pixel(74, 25), QD_COLOR_YELLOW);
    ASSERT_EQ(qd_get_pixel(25, 64), QD_COLOR_YELLOW);
    ASSERT_EQ(qd_get_pixel(74, 64), QD_COLOR_YELLOW);
    ASSERT_EQ(qd_get_pixel(50, 45), QD_COLOR_YELLOW);
    ASSERT_EQ(qd_get_pixel(0, 0), QD_COLOR_BLACK);
}

TEST(quickdraw_screen_clear) {
    setup_quickdraw_system();

    qd_set_pixel(10, 10, QD_COLOR_WHITE);
    qd_set_pixel(20, 20, QD_COLOR_RED);
    qd_fill_rect(50, 50, 30, 20, QD_COLOR_BLUE);

    ASSERT_EQ(qd_get_pixel(10, 10), QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(20, 20), QD_COLOR_RED);
    ASSERT_EQ(qd_get_pixel(65, 60), QD_COLOR_BLUE);

    qd_clear_screen(QD_COLOR_BLACK);

    ASSERT_EQ(qd_get_pixel(10, 10), QD_COLOR_BLACK);
    ASSERT_EQ(qd_get_pixel(20, 20), QD_COLOR_BLACK);
    ASSERT_EQ(qd_get_pixel(65, 60), QD_COLOR_BLACK);
}

TEST(quickdraw_color_operations) {
    setup_quickdraw_system();

    qd_set_pixel(0, 0, QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(0, 0), QD_COLOR_WHITE);

    qd_set_pixel(1, 0, QD_COLOR_RED);
    ASSERT_EQ(qd_get_pixel(1, 0), QD_COLOR_RED);

    qd_set_pixel(2, 0, QD_COLOR_GREEN);
    ASSERT_EQ(qd_get_pixel(2, 0), QD_COLOR_GREEN);

    qd_set_pixel(3, 0, QD_COLOR_BLUE);
    ASSERT_EQ(qd_get_pixel(3, 0), QD_COLOR_BLUE);

    qd_set_pixel(4, 0, QD_COLOR_YELLOW);
    ASSERT_EQ(qd_get_pixel(4, 0), QD_COLOR_YELLOW);

    qd_set_pixel(5, 0, QD_COLOR_CYAN);
    ASSERT_EQ(qd_get_pixel(5, 0), QD_COLOR_CYAN);

    qd_set_pixel(6, 0, QD_COLOR_MAGENTA);
    ASSERT_EQ(qd_get_pixel(6, 0), QD_COLOR_MAGENTA);
}

TEST(quickdraw_boundary_validation) {
    setup_quickdraw_system();

    qd_set_pixel(0, 0, QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(0, 0), QD_COLOR_WHITE);

    qd_set_pixel(QD_SCREEN_WIDTH - 1, 0, QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(QD_SCREEN_WIDTH - 1, 0), QD_COLOR_WHITE);

    qd_set_pixel(0, QD_SCREEN_HEIGHT - 1, QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(0, QD_SCREEN_HEIGHT - 1), QD_COLOR_WHITE);

    qd_set_pixel(QD_SCREEN_WIDTH - 1, QD_SCREEN_HEIGHT - 1, QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(QD_SCREEN_WIDTH - 1, QD_SCREEN_HEIGHT - 1), QD_COLOR_WHITE);

    qd_set_pixel(QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT, QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT), QD_COLOR_BLACK);
}

TEST(quickdraw_vram_operations) {
    setup_quickdraw_system();

    uint8_t custom_vram[QD_VRAM_BYTES] = {0};
    qd_set_vram_buffer(custom_vram);
    qd_clear_screen(QD_COLOR_BLACK);
    qd_set_pixel(50, 50, QD_COLOR_RED);
    ASSERT_EQ(qd_get_pixel(50, 50), QD_COLOR_RED);

    qd_set_vram_buffer(test_vram);
    qd_clear_screen(QD_COLOR_BLACK);
    qd_set_pixel(100, 100, QD_COLOR_BLUE);
    ASSERT_EQ(qd_get_pixel(100, 100), QD_COLOR_BLUE);
    ASSERT_EQ(qd_get_pixel(50, 50), QD_COLOR_BLACK);
}

TEST(quickdraw_performance_basic) {
    setup_quickdraw_system();

    for (int y = 0; y < 100; y++) {
        for (int x = 0; x < 100; x++) {
            qd_set_pixel(x, y, (uint8_t)((x + y) % 16));
        }
    }

    ASSERT_EQ(qd_get_pixel(0, 0), 0);
    ASSERT_EQ(qd_get_pixel(50, 50), (uint8_t)((50 + 50) % 16));
    ASSERT_EQ(qd_get_pixel(99, 99), (uint8_t)((99 + 99) % 16));
}

TEST(quickdraw_line_operations) {
    setup_quickdraw_system();

    qd_draw_line(10, 20, 59, 20, QD_COLOR_WHITE);
    for (int x = 10; x < 60; x++) {
        ASSERT_EQ(qd_get_pixel(x, 20), QD_COLOR_WHITE);
    }

    qd_draw_line(30, 10, 30, 49, QD_COLOR_RED);
    for (int y = 10; y < 50; y++) {
        ASSERT_EQ(qd_get_pixel(30, y), QD_COLOR_RED);
    }

    ASSERT_EQ(qd_get_pixel(0, 0), QD_COLOR_BLACK);
    ASSERT_EQ(qd_get_pixel(100, 100), QD_COLOR_BLACK);
}

TEST(quickdraw_text_rendering) {
    setup_quickdraw_system();
    setup_font_stub();

    ASSERT_EQ(qd_get_font_width(), 8);
    ASSERT_EQ(qd_get_font_height(), 16);
    ASSERT_EQ(qd_measure_text("A"), 8);
    ASSERT_EQ(qd_measure_text("AB"), 16);
    ASSERT_EQ(qd_measure_text("A\nB"), 8);

    qd_clear_screen(QD_COLOR_BLACK);
    qd_draw_char(10, 12, 'A', QD_COLOR_RED, QD_COLOR_BLUE, true);

    ASSERT_EQ(qd_get_pixel(10, 12), QD_COLOR_BLUE);
    ASSERT_EQ(qd_get_pixel(11, 12), QD_COLOR_RED);
    ASSERT_EQ(qd_get_pixel(17, 12), QD_COLOR_BLUE);
    ASSERT_EQ(qd_get_pixel(10, 20), QD_COLOR_RED);
    ASSERT_EQ(qd_get_pixel(13, 20), QD_COLOR_BLUE);

    qd_draw_text(40, 40, "A\nB", QD_COLOR_WHITE, QD_COLOR_BLACK, true);

    ASSERT_EQ(qd_get_pixel(40, 40), QD_COLOR_BLACK);
    ASSERT_EQ(qd_get_pixel(41, 40), QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(41, 56), QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(47, 56), QD_COLOR_BLACK);
}

void run_quickdraw_tests(void) {
    RUN_TEST(quickdraw_initialization_basic);
    RUN_TEST(quickdraw_pixel_operations);
    RUN_TEST(quickdraw_rectangle_operations);
    RUN_TEST(quickdraw_fill_rect_operations);
    RUN_TEST(quickdraw_screen_clear);
    RUN_TEST(quickdraw_color_operations);
    RUN_TEST(quickdraw_boundary_validation);
    RUN_TEST(quickdraw_vram_operations);
    RUN_TEST(quickdraw_performance_basic);
    RUN_TEST(quickdraw_line_operations);
    RUN_TEST(quickdraw_text_rendering);
}
