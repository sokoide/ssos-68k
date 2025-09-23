/**
 * @file quickdraw_demo.c
 * @brief QuickDrawシステムのデモプログラム
 *
 * QuickDrawシステムの基本的な描画機能をテスト・デモンストレーション。
 */

#include "quickdraw.h"

// デモ関数のプロトタイプ
void demo_basic_drawing(void);
void demo_rectangles(void);
void demo_lines(void);
void demo_text(void);
void demo_colors(void);

/**
 * @brief QuickDrawシステムのデモを実行
 */
void run_quickdraw_demo(void) {
    // QuickDrawライブラリ自体を無効化（アドレスエラー調査用）
    // qd_init_minimal();

    // VRAM書き込みは無効化（アドレスエラー調査用）
    // demo_basic_drawing();
    // demo_lines();
    // demo_text();
    // demo_rectangles();
    // demo_colors();
}

/**
 * @brief 基本的な描画デモ
 */
void demo_basic_drawing(void) {
    // VRAM書き込みを無効化（アドレスエラー調査用）
    // qd_clear_screen(QD_COLOR_BLACK);
    // qd_set_pixel_fast(50, 50, QD_COLOR_WHITE);
    // qd_set_pixel_fast(50, 52, QD_COLOR_WHITE);
}

/**
 * @brief 矩形描画デモ（簡素化版）
 */
void demo_rectangles(void) {
    // VRAM書き込みを無効化（アドレスエラー調査用）
    // qd_fill_rect(100, 100, 20, 15, QD_COLOR_BLUE);
}

/**
 * @brief 線描画デモ（簡素化版）
 */
void demo_lines(void) {
    // VRAM書き込みを無効化（アドレスエラー調査用）
    // qd_draw_line(100, 300, 200, 300, QD_COLOR_CYAN);
}

/**
 * @brief テキスト描画デモ（簡素化版）
 */
void demo_text(void) {
    // VRAM書き込みを無効化（アドレスエラー調査用）
    // qd_draw_text(50, 400, "QuickDraw", QD_COLOR_WHITE, QD_COLOR_BLACK);
}

/**
 * @brief カラー表示デモ（簡素化版）
 */
void demo_colors(void) {
    // VRAM書き込みを無効化（アドレスエラー調査用）
    // qd_fill_rect(x, y, 25, 25, QD_COLOR_WHITE);
    // qd_draw_text(x + 5, y + 5, "15", QD_COLOR_BLACK, QD_COLOR_WHITE);
}