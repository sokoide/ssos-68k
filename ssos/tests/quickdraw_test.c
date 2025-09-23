/**
 * @file quickdraw_test.c
 * @brief QuickDrawスタイル API の簡易テストランナー
 */

#include "../os/window/quickdraw.h"
#include "../os/util/printf.h"
#include <stdint.h>
#include <stdlib.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(condition, message)             \
    do {                                            \
        if (condition) {                            \
            g_tests_passed++;                       \
            printf("\u2713 PASS: %s\n", message);   \
        } else {                                    \
            g_tests_failed++;                       \
            printf("\u2717 FAIL: %s\n", message);   \
        }                                           \
    } while (0)

static void test_pixel_operations(void) {
    printf("\n=== ピクセル操作テスト ===\n");

    qd_init();

    qd_set_pixel(100, 50, QD_COLOR_RED);
    TEST_ASSERT(qd_get_pixel(100, 50) == QD_COLOR_RED, "ピクセル描画/取得");

    qd_set_pixel(QD_SCREEN_WIDTH + 1, 10, QD_COLOR_BLUE);
    TEST_ASSERT(qd_get_pixel(QD_SCREEN_WIDTH + 1, 10) == 0, "画面外読み取り安全性");
}

static void test_rect_operations(void) {
    printf("\n=== 矩形描画テスト ===\n");

    qd_clear_screen(QD_COLOR_BLACK);
    qd_fill_rect(50, 100, 60, 40, QD_COLOR_GREEN);

    TEST_ASSERT(qd_get_pixel(50, 100) == QD_COLOR_GREEN, "矩形塗りつぶし 左上");
    TEST_ASSERT(qd_get_pixel(109, 139) == QD_COLOR_GREEN, "矩形塗りつぶし 右下");

    qd_draw_rect(150, 100, 40, 30, QD_COLOR_WHITE);
    TEST_ASSERT(qd_get_pixel(150, 100) == QD_COLOR_WHITE, "矩形枠線 上辺");
}

static void test_screen_clear(void) {
    printf("\n=== 画面クリアテスト ===\n");

    qd_fill_rect(0, 0, 100, 100, QD_COLOR_YELLOW);
    qd_clear_screen(QD_COLOR_BLACK);

    int cleared = 0;
    int total = 0;
    for (int y = 0; y < QD_SCREEN_HEIGHT; y += 64) {
        for (int x = 0; x < QD_SCREEN_WIDTH; x += 64) {
            if (qd_get_pixel(x, y) == QD_COLOR_BLACK) {
                cleared++;
            }
            total++;
        }
    }
    TEST_ASSERT(cleared == total, "画面クリア 全域");
}

static void test_clipping(void) {
    printf("\n=== クリッピングテスト ===\n");

    qd_clear_screen(QD_COLOR_BLACK);
    qd_set_clip_rect(100, 100, 100, 80);
    qd_fill_rect(50, 50, 200, 160, QD_COLOR_CYAN);

    TEST_ASSERT(qd_get_pixel(100, 100) == QD_COLOR_CYAN, "クリップ内描画");
    TEST_ASSERT(qd_get_pixel(60, 60) == QD_COLOR_BLACK, "クリップ外非描画");

    qd_set_clip_rect(0, 0, QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT);
}

static void test_line_drawing(void) {
    printf("\n=== 線描画テスト ===\n");

    qd_clear_screen(QD_COLOR_BLACK);
    qd_draw_line(10, 10, 70, 10, QD_COLOR_WHITE);
    TEST_ASSERT(qd_get_pixel(10, 10) == QD_COLOR_WHITE, "水平線 始点");
    TEST_ASSERT(qd_get_pixel(70, 10) == QD_COLOR_WHITE, "水平線 終点");

    qd_draw_line(20, 20, 20, 60, QD_COLOR_RED);
    TEST_ASSERT(qd_get_pixel(20, 20) == QD_COLOR_RED, "垂直線 始点");
    TEST_ASSERT(qd_get_pixel(20, 60) == QD_COLOR_RED, "垂直線 終点");
}

int main(void) {
    printf("QuickDrawテストスイート開始\n");
    printf("==============================\n");

    test_pixel_operations();
    test_rect_operations();
    test_screen_clear();
    test_clipping();
    test_line_drawing();

    printf("\n==============================\n");
    printf("成功: %d\n", g_tests_passed);
    printf("失敗: %d\n", g_tests_failed);
    printf("合計: %d\n", g_tests_passed + g_tests_failed);

    if (g_tests_failed == 0) {
        printf("\u2713 すべてのテストが成功しました！\n");
        return 0;
    }
    printf("\u2717 %d 個のテストが失敗しました\n", g_tests_failed);
    return 1;
}
