/**
 * @file quickdraw_test.c
 * @brief QuickDrawシステムのテストプログラム
 *
 * 基本的な描画機能が正しく動作することを確認するテスト。
 */

#include "../os/window/quickdraw.h"
#include "../os/util/printf.h"
#include <stdint.h>
#include <stdlib.h>

// テストカウンター
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    do { \
        if (condition) { \
            g_tests_passed++; \
            printf("✓ PASS: %s\n", message); \
        } else { \
            g_tests_failed++; \
            printf("✗ FAIL: %s\n", message); \
        } \
    } while (0)

/**
 * @brief ピクセル描画テスト
 */
void test_pixel_operations(void) {
    printf("\n=== ピクセル操作テスト ===\n");

    // システム初期化
    qd_init();

    // テスト用座標
    uint16_t test_x = 100;
    uint16_t test_y = 50;
    uint8_t test_color = QD_COLOR_RED;

    // ピクセル描画テスト
    qd_set_pixel(test_x, test_y, test_color);
    uint8_t read_color = qd_get_pixel(test_x, test_y);
    TEST_ASSERT(read_color == test_color, "ピクセル描画・読み取り");

    // 高速ピクセル描画テスト
    qd_set_pixel_fast(test_x + 1, test_y, QD_COLOR_BLUE);
    read_color = qd_get_pixel(test_x + 1, test_y);
    TEST_ASSERT(read_color == QD_COLOR_BLUE, "高速ピクセル描画");

    // 画面外アクセステスト（エラーハンドリング）
    qd_set_pixel(QD_SCREEN_WIDTH + 10, test_y, test_color);
    qd_set_pixel(test_x, QD_SCREEN_HEIGHT + 10, test_color);
    TEST_ASSERT(qd_get_pixel(QD_SCREEN_WIDTH + 10, test_y) == 0, "画面外アクセス処理");

    printf("ピクセル操作テスト完了\n");
}

/**
 * @brief 矩形描画テスト
 */
void test_rect_operations(void) {
    printf("\n=== 矩形描画テスト ===\n");

    // 矩形塗りつぶしテスト
    uint16_t rect_x = 50;
    uint16_t rect_y = 100;
    uint16_t rect_width = 60;
    uint16_t rect_height = 40;
    uint8_t rect_color = QD_COLOR_GREEN;

    qd_fill_rect(rect_x, rect_y, rect_width, rect_height, rect_color);

    // 矩形の隅をチェック
    TEST_ASSERT(qd_get_pixel(rect_x, rect_y) == rect_color, "矩形塗りつぶし (左上)");
    TEST_ASSERT(qd_get_pixel(rect_x + rect_width - 1, rect_y) == rect_color, "矩形塗りつぶし (右上)");
    TEST_ASSERT(qd_get_pixel(rect_x, rect_y + rect_height - 1) == rect_color, "矩形塗りつぶし (左下)");
    TEST_ASSERT(qd_get_pixel(rect_x + rect_width - 1, rect_y + rect_height - 1) == rect_color, "矩形塗りつぶし (右下)");

    // 矩形描画（枠線）テスト
    qd_draw_rect(rect_x + 100, rect_y, rect_width, rect_height, QD_COLOR_WHITE);
    TEST_ASSERT(qd_get_pixel(rect_x + 100, rect_y) == QD_COLOR_WHITE, "矩形枠線 (上辺)");
    TEST_ASSERT(qd_get_pixel(rect_x + 100 + rect_width - 1, rect_y) == QD_COLOR_WHITE, "矩形枠線 (右上)");

    printf("矩形描画テスト完了\n");
}

/**
 * @brief 画面クリアテスト
 */
void test_screen_clear(void) {
    printf("\n=== 画面クリアテスト ===\n");

    // 画面クリア
    qd_clear_screen(QD_COLOR_BLACK);

    // 画面全体がクリアされたか確認（サンプリングテスト）
    int clear_count = 0;
    int test_count = 0;

    for (int y = 0; y < QD_SCREEN_HEIGHT; y += 50) {
        for (int x = 0; x < QD_SCREEN_WIDTH; x += 100) {
            if (qd_get_pixel(x, y) == QD_COLOR_BLACK) {
                clear_count++;
            }
            test_count++;
        }
    }

    TEST_ASSERT(clear_count == test_count, "画面クリア");

    // 部分クリアテスト
    qd_fill_rect(200, 200, 100, 80, QD_COLOR_YELLOW);
    qd_clear_rect(200, 200, 100, 80, QD_COLOR_BLACK);

    TEST_ASSERT(qd_get_pixel(200, 200) == QD_COLOR_BLACK, "部分クリア");
    TEST_ASSERT(qd_get_pixel(250, 240) == QD_COLOR_BLACK, "部分クリア (中央)");

    printf("画面クリアテスト完了\n");
}

/**
 * @brief クリッピングテスト
 */
void test_clipping(void) {
    printf("\n=== クリッピングテスト ===\n");

    // クリッピング矩形設定
    qd_set_clip_rect(100, 100, 200, 150);

    // クリッピング外の領域に描画
    qd_fill_rect(50, 50, 100, 100, QD_COLOR_RED);  // 部分的にクリッピング外
    qd_fill_rect(150, 150, 100, 100, QD_COLOR_BLUE); // 部分的にクリッピング外

    // クリッピング内は描画されていることを確認
    TEST_ASSERT(qd_get_pixel(150, 150) == QD_COLOR_BLUE, "クリッピング内描画");

    // クリッピング外は描画されていないことを確認
    TEST_ASSERT(qd_get_pixel(50, 50) == QD_COLOR_BLACK, "クリッピング外非描画");

    printf("クリッピングテスト完了\n");
}

/**
 * @brief 線描画テスト
 */
void test_line_drawing(void) {
    printf("\n=== 線描画テスト ===\n");

    // 水平線
    qd_draw_line(300, 100, 400, 100, QD_COLOR_CYAN);
    TEST_ASSERT(qd_get_pixel(300, 100) == QD_COLOR_CYAN, "水平線描画 (始点)");
    TEST_ASSERT(qd_get_pixel(400, 100) == QD_COLOR_CYAN, "水平線描画 (終点)");

    // 垂直線
    qd_draw_line(350, 120, 350, 200, QD_COLOR_MAGENTA);
    TEST_ASSERT(qd_get_pixel(350, 120) == QD_COLOR_MAGENTA, "垂直線描画 (始点)");
    TEST_ASSERT(qd_get_pixel(350, 200) == QD_COLOR_MAGENTA, "垂直線描画 (終点)");

    // 対角線
    qd_draw_line(300, 300, 400, 350, QD_COLOR_YELLOW);
    TEST_ASSERT(qd_get_pixel(300, 300) == QD_COLOR_YELLOW, "対角線描画 (始点)");
    TEST_ASSERT(qd_get_pixel(400, 350) == QD_COLOR_YELLOW, "対角線描画 (終点)");

    printf("線描画テスト完了\n");
}

/**
 * @brief パレットテスト
 */
void test_palette(void) {
    printf("\n=== パレットテスト ===\n");

    // デフォルトパレット初期化
    qd_init_default_palette();

    // パレットカラーの取得テスト
    uint16_t black_palette = qd_get_palette(QD_COLOR_BLACK);
    uint16_t white_palette = qd_get_palette(QD_COLOR_WHITE);

    TEST_ASSERT(black_palette == 0x0000, "ブラックパレット値");
    TEST_ASSERT(white_palette == 0xFFF0, "ホワイトパレット値");

    // カスタムパレット設定
    qd_set_palette(8, 0xABC0);  // カスタムグレー
    uint16_t custom_palette = qd_get_palette(8);
    TEST_ASSERT(custom_palette == 0xABC0, "カスタムパレット設定");

    printf("パレットテスト完了\n");
}

/**
 * @brief システム情報テスト
 */
void test_system_info(void) {
    printf("\n=== システム情報テスト ===\n");

    // システム情報表示
    qd_print_info();

    // VRAMサイズ確認
    uint32_t vram_size = qd_get_vram_size();
    TEST_ASSERT(vram_size == (QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2, "VRAMサイズ計算");

    // クリッピング矩形取得
    QD_Rect* clip_rect = qd_get_clip_rect();
    TEST_ASSERT(clip_rect->width == QD_SCREEN_WIDTH && clip_rect->height == QD_SCREEN_HEIGHT,
                "デフォルトクリッピング矩形");

    printf("システム情報テスト完了\n");
}

/**
 * @brief メインテスト関数
 */
int main(void) {
    printf("QuickDrawシステム テストスイート開始\n");
    printf("=====================================\n");

    // すべてのテストを実行
    test_system_info();
    test_pixel_operations();
    test_rect_operations();
    test_screen_clear();
    test_clipping();
    test_line_drawing();
    test_palette();

    // 結果サマリー
    printf("\n=====================================\n");
    printf("テスト結果サマリー:\n");
    printf("成功: %d\n", g_tests_passed);
    printf("失敗: %d\n", g_tests_failed);
    printf("合計: %d\n", g_tests_passed + g_tests_failed);

    if (g_tests_failed == 0) {
        printf("✓ すべてのテストが成功しました！\n");
        return 0;
    } else {
        printf("✗ %d個のテストが失敗しました\n", g_tests_failed);
        return 1;
    }
}