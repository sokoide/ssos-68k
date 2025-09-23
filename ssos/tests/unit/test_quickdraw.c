#include "../framework/ssos_test.h"

// Include QuickDraw graphics system
#include "../../os/window/quickdraw.h"

// Test VRAM buffer for QuickDraw operations
static uint16_t test_vram[QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT / 2] __attribute__((aligned(4)));

// Test helper: Initialize QuickDraw system
static void setup_quickdraw_system(void) {
    qd_init();
    qd_set_vram_buffer(test_vram);
    qd_clear_screen(QD_COLOR_BLACK);
}

// Test helper: Reset VRAM buffer to clean state
static void reset_vram_buffer(void) {
    // Initialize VRAM to zero (all black)
    for (uint32_t i = 0; i < sizeof(test_vram) / sizeof(uint16_t); i++) {
        test_vram[i] = 0;
    }
}

// Test basic QuickDraw initialization
TEST(quickdraw_initialization_basic) {
    qd_init();

    ASSERT_TRUE(qd_is_initialized());
    ASSERT_EQ(qd_get_screen_width(), QD_SCREEN_WIDTH);
    ASSERT_EQ(qd_get_screen_height(), QD_SCREEN_HEIGHT);
}

// Test pixel operations
TEST(quickdraw_pixel_operations) {
    setup_quickdraw_system();

    // Debug: Print VRAM buffer info
    printf("VRAM buffer: %p, test_vram: %p\n", qd_get_vram_buffer(), test_vram);

    // Test pixel setting with detailed debug
    printf("Setting pixel (100,100) to color %d (WHITE)\n", QD_COLOR_WHITE);
    qd_set_pixel(100, 100, QD_COLOR_WHITE);

    // Check VRAM directly
    uint32_t offset = (100 * QD_SCREEN_WIDTH + 100) / 2;
    uint16_t* vram = qd_get_vram_buffer();
    uint16_t vram_word = vram[offset];
    printf("VRAM offset: %d, word value: 0x%04X\n", offset, vram_word);

    uint8_t result = qd_get_pixel(100, 100);
    printf("Get pixel (100,100) returned: %d\n", result);
    ASSERT_EQ(result, QD_COLOR_WHITE);

    // Test pixel setting in different positions
    qd_set_pixel(0, 0, QD_COLOR_RED);
    result = qd_get_pixel(0, 0);
    printf("Set pixel (0,0) to %d, got %d\n", QD_COLOR_RED, result);
    ASSERT_EQ(result, QD_COLOR_RED);

    qd_set_pixel(QD_SCREEN_WIDTH - 1, QD_SCREEN_HEIGHT - 1, QD_COLOR_BLUE);
    result = qd_get_pixel(QD_SCREEN_WIDTH - 1, QD_SCREEN_HEIGHT - 1);
    printf("Set pixel (max,max) to %d, got %d\n", QD_COLOR_BLUE, result);
    ASSERT_EQ(result, QD_COLOR_BLUE);

    // Test boundary checking
    ASSERT_EQ(qd_get_pixel(QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT), QD_COLOR_BLACK); // Out of bounds
}

// Test rectangle operations
TEST(quickdraw_rectangle_operations) {
    reset_vram_buffer();
    qd_set_vram_buffer(test_vram);

    // Test rectangle drawing
    qd_draw_rect(50, 50, 100, 80, QD_COLOR_GREEN);

    // Check corners of the rectangle
    ASSERT_EQ(qd_get_pixel(50, 50), QD_COLOR_GREEN);     // Top-left
    ASSERT_EQ(qd_get_pixel(149, 50), QD_COLOR_GREEN);    // Top-right
    ASSERT_EQ(qd_get_pixel(50, 129), QD_COLOR_GREEN);    // Bottom-left
    ASSERT_EQ(qd_get_pixel(149, 129), QD_COLOR_GREEN);   // Bottom-right

    // Check inside and outside the rectangle
    ASSERT_EQ(qd_get_pixel(100, 100), QD_COLOR_BLACK);   // Inside (not filled)
    ASSERT_EQ(qd_get_pixel(25, 25), QD_COLOR_BLACK);     // Outside
}

// Test rectangle fill operations
TEST(quickdraw_fill_rect_operations) {
    reset_vram_buffer();
    qd_set_vram_buffer(test_vram);

    // Test rectangle filling
    qd_fill_rect(25, 25, 50, 40, QD_COLOR_YELLOW);

    // Check filled area
    ASSERT_EQ(qd_get_pixel(25, 25), QD_COLOR_YELLOW);     // Top-left
    ASSERT_EQ(qd_get_pixel(74, 25), QD_COLOR_YELLOW);     // Top-right
    ASSERT_EQ(qd_get_pixel(25, 64), QD_COLOR_YELLOW);     // Bottom-left
    ASSERT_EQ(qd_get_pixel(74, 64), QD_COLOR_YELLOW);     // Bottom-right
    ASSERT_EQ(qd_get_pixel(50, 45), QD_COLOR_YELLOW);     // Center

    // Check outside the filled area
    ASSERT_EQ(qd_get_pixel(0, 0), QD_COLOR_BLACK);
    ASSERT_EQ(qd_get_pixel(100, 100), QD_COLOR_BLACK);
}

// Test screen clearing
TEST(quickdraw_screen_clear) {
    reset_vram_buffer();
    qd_set_vram_buffer(test_vram);

    // Draw some content
    qd_set_pixel(10, 10, QD_COLOR_WHITE);
    qd_set_pixel(20, 20, QD_COLOR_RED);
    qd_fill_rect(50, 50, 30, 20, QD_COLOR_BLUE);

    // Verify content was drawn
    ASSERT_EQ(qd_get_pixel(10, 10), QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(20, 20), QD_COLOR_RED);
    ASSERT_EQ(qd_get_pixel(65, 60), QD_COLOR_BLUE);

    // Clear screen
    qd_clear_screen(QD_COLOR_BLACK);

    // Verify screen was cleared
    ASSERT_EQ(qd_get_pixel(10, 10), QD_COLOR_BLACK);
    ASSERT_EQ(qd_get_pixel(20, 20), QD_COLOR_BLACK);
    ASSERT_EQ(qd_get_pixel(65, 60), QD_COLOR_BLACK);
}

// Test color operations
TEST(quickdraw_color_operations) {
    reset_vram_buffer();
    qd_set_vram_buffer(test_vram);

    // Test setting different colors
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

// Test boundary checking and validation
TEST(quickdraw_boundary_validation) {
    reset_vram_buffer();
    qd_set_vram_buffer(test_vram);

    // Test operations at screen boundaries
    qd_set_pixel(0, 0, QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(0, 0), QD_COLOR_WHITE);

    qd_set_pixel(QD_SCREEN_WIDTH - 1, 0, QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(QD_SCREEN_WIDTH - 1, 0), QD_COLOR_WHITE);

    qd_set_pixel(0, QD_SCREEN_HEIGHT - 1, QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(0, QD_SCREEN_HEIGHT - 1), QD_COLOR_WHITE);

    qd_set_pixel(QD_SCREEN_WIDTH - 1, QD_SCREEN_HEIGHT - 1, QD_COLOR_WHITE);
    ASSERT_EQ(qd_get_pixel(QD_SCREEN_WIDTH - 1, QD_SCREEN_HEIGHT - 1), QD_COLOR_WHITE);

    // Test operations outside boundaries (should not crash)
    qd_set_pixel(QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT, QD_COLOR_WHITE); // Out of bounds
    // Should not crash, and should return black for out-of-bounds reads
    ASSERT_EQ(qd_get_pixel(QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT), QD_COLOR_BLACK);
}

// Test VRAM buffer operations
TEST(quickdraw_vram_operations) {
    uint16_t custom_vram[QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT / 2] __attribute__((aligned(4)));

    // Test custom VRAM buffer
    qd_init();
    qd_set_vram_buffer(custom_vram);

    // Draw to custom buffer
    qd_set_pixel(50, 50, QD_COLOR_RED);
    ASSERT_EQ(qd_get_pixel(50, 50), QD_COLOR_RED);

    // Switch back to default buffer
    qd_set_vram_buffer(test_vram);
    qd_clear_screen(QD_COLOR_BLACK);
    qd_set_pixel(100, 100, QD_COLOR_BLUE);
    ASSERT_EQ(qd_get_pixel(100, 100), QD_COLOR_BLUE);
    ASSERT_EQ(qd_get_pixel(50, 50), QD_COLOR_BLACK); // Should be in different buffer
}

// Test performance of basic operations
TEST(quickdraw_performance_basic) {
    reset_vram_buffer();
    qd_set_vram_buffer(test_vram);

    // Test bulk pixel operations performance
    for (int y = 0; y < 100; y++) {
        for (int x = 0; x < 100; x++) {
            qd_set_pixel(x, y, (x + y) % 16);
        }
    }

    // Verify some pixels were set correctly
    ASSERT_EQ(qd_get_pixel(0, 0), 0);
    ASSERT_EQ(qd_get_pixel(50, 50), (50 + 50) % 16);
    ASSERT_EQ(qd_get_pixel(99, 99), (99 + 99) % 16);
}

// Test line drawing (basic horizontal/vertical)
TEST(quickdraw_line_operations) {
    reset_vram_buffer();
    qd_set_vram_buffer(test_vram);

    // Test horizontal line using draw_line
    qd_draw_line(10, 20, 59, 20, QD_COLOR_WHITE);
    for (int x = 10; x < 60; x++) {
        ASSERT_EQ(qd_get_pixel(x, 20), QD_COLOR_WHITE);
    }

    // Test vertical line using draw_line
    qd_draw_line(30, 10, 30, 49, QD_COLOR_RED);
    for (int y = 10; y < 50; y++) {
        ASSERT_EQ(qd_get_pixel(30, y), QD_COLOR_RED);
    }

    // Verify non-line pixels unchanged
    ASSERT_EQ(qd_get_pixel(0, 0), QD_COLOR_BLACK);
    ASSERT_EQ(qd_get_pixel(100, 100), QD_COLOR_BLACK);
}

// Run all QuickDraw tests
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
}