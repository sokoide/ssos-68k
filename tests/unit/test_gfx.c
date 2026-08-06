#include "ssos_test.h"
#include "gfx.h"

static uint32_t stride(void) {
    return (uint32_t)ss_current_mode->bytes_per_line / 2;
}

static uint16_t pixel(int x, int y) {
    return ss_draw_page[(uint32_t)y * stride() + (uint32_t)x];
}

static void reset_gfx(int mode, uint16_t color) {
    ss_gfx_set_mode(mode);
    ss_gfx_init();
    ss_gfx_clear(color);
}

TEST(gfx_set_mode_rejects_unimplemented_values) {
    ss_gfx_set_mode(SS_CRTMOD_16);
    const SSGfxMode* mode16 = ss_current_mode;
    ss_gfx_set_mode(9);
    ASSERT_EQ(ss_current_mode, mode16);
    ss_gfx_set_mode(SS_CRTMOD_8);
    ASSERT_NEQ(ss_current_mode, mode16);
}

TEST(gfx_rect_clips_and_preserves_outside) {
    reset_gfx(SS_CRTMOD_16, 0x1111);
    ss_gfx_rect(-2, -1, 4, 3, 0x2222);
    ASSERT_EQ(pixel(0, 0), 0x2222);
    ASSERT_EQ(pixel(1, 1), 0x2222);
    ASSERT_EQ(pixel(2, 0), 0x1111);
    ASSERT_EQ(pixel(0, 2), 0x1111);
}

TEST(gfx_rect_handles_odd_alignment) {
    reset_gfx(SS_CRTMOD_16, 0x1111);
    ss_gfx_rect(3, 4, 5, 1, 0x2222);
    ASSERT_EQ(pixel(2, 4), 0x1111);
    for (int x = 3; x < 8; x++) ASSERT_EQ(pixel(x, 4), 0x2222);
    ASSERT_EQ(pixel(8, 4), 0x1111);
}

TEST(gfx_stipple_phase_and_clip) {
    reset_gfx(SS_CRTMOD_16, 0x1111);
    ss_gfx_fill_stipple(-1, 1, 3, 2, 0x2222, 0x3333);
    ASSERT_EQ(pixel(0, 1), 0x3333);
    ASSERT_EQ(pixel(1, 1), 0x2222);
    ASSERT_EQ(pixel(0, 2), 0x2222);
    ASSERT_EQ(pixel(1, 2), 0x3333);
    ASSERT_EQ(pixel(2, 1), 0x1111);
}

TEST(gfx_region_intersects_rect) {
    reset_gfx(SS_CRTMOD_16, 0x1111);
    SSGfxRect clip = { 4, 3, 3, 2 };
    ss_gfx_rect_region((SSGfxRect){ 2, 2, 5, 4 }, &clip, 0x2222);
    ASSERT_EQ(pixel(4, 3), 0x2222);
    ASSERT_EQ(pixel(6, 4), 0x2222);
    ASSERT_EQ(pixel(3, 3), 0x1111);
    ASSERT_EQ(pixel(4, 2), 0x1111);
}

TEST(gfx_char_fast_matches_slow) {
    reset_gfx(SS_CRTMOD_16, 0x1111);
    ss_gfx_char(20, 30, 'A', 0x2222, 0x3333);
    uint16_t expected[SS_FONT_W * SS_FONT_H];
    for (int y = 0; y < SS_FONT_H; y++) {
        for (int x = 0; x < SS_FONT_W; x++) expected[y * SS_FONT_W + x] = pixel(20 + x, 30 + y);
    }
    ss_gfx_clear(0x1111);
    ss_gfx_char_fast(20, 30, 'A', 0x2222, 0x3333);
    for (int y = 0; y < SS_FONT_H; y++) {
        for (int x = 0; x < SS_FONT_W; x++) ASSERT_EQ(pixel(20 + x, 30 + y), expected[y * SS_FONT_W + x]);
    }
}

TEST(gfx_xor_perimeter_twice_restores) {
    reset_gfx(SS_CRTMOD_16, 0x1111);
    ss_gfx_rect(10, 10, 4, 3, 0x2468);
    ss_gfx_xor_rect(10, 10, 4, 3);
    ASSERT_EQ(pixel(10, 10), (uint16_t)(0x2468 ^ 0xffff));
    ASSERT_EQ(pixel(11, 11), 0x2468);
    ss_gfx_xor_rect(10, 10, 4, 3);
    for (int y = 10; y < 13; y++) {
        for (int x = 10; x < 14; x++) ASSERT_EQ(pixel(x, y), 0x2468);
    }
}

TEST(gfx_flip_switches_pages) {
    reset_gfx(SS_CRTMOD_8, 0x1111);
    ss_gfx_rect(0, 0, 1, 1, 0x2222);
    volatile uint16_t* first = ss_draw_page;
    ss_gfx_flip();
    ASSERT_EQ(ss_display_page, first);
    ASSERT_NEQ(ss_draw_page, first);
    ASSERT_EQ(ss_gfx_test_crtc[SS_CRTC_SCROLL_Y], 0);
    ss_gfx_rect(0, 0, 1, 1, 0x3333);
    ss_gfx_flip();
    ASSERT_EQ(ss_display_page[0], 0x3333);
    ASSERT_EQ(ss_gfx_test_crtc[SS_CRTC_SCROLL_Y], 512);
}

void run_gfx_tests(void) {
    RUN_TEST(gfx_set_mode_rejects_unimplemented_values);
    RUN_TEST(gfx_rect_clips_and_preserves_outside);
    RUN_TEST(gfx_rect_handles_odd_alignment);
    RUN_TEST(gfx_stipple_phase_and_clip);
    RUN_TEST(gfx_region_intersects_rect);
    RUN_TEST(gfx_char_fast_matches_slow);
    RUN_TEST(gfx_xor_perimeter_twice_restores);
    RUN_TEST(gfx_flip_switches_pages);
}
