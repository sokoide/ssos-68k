/**
 * @file quickdraw.h
 * @brief QuickDrawスタイルグラフィックスシステム - 68000 16MHz最適化
 *
 * 現在の複雑なレイヤーシステムを置き換えるシンプルで高速なグラフィックスAPI。
 * 68000 16MHz環境での16色カラー表示に最適化された直接描画システム。
 *
 * @section 設計コンセプト
 * - 直接VRAM描画（オフスクリーンバッファ不要）
 * - 16色パレットベース
 * - 整数座標系による高速演算
 * - 最小限のAPIで最大限の機能
 *
 * @section パフォーマンス最適化
 * - ワードアクセス（2ピクセル同時転送）
 * - クリッピングによる描画範囲最適化
 * - メモリアライメント考慮
 *
 * @author SSOS Development Team
 * @date 2025
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

// 画面解像度定義（768x512の16色モード）
#define QD_SCREEN_WIDTH     768
#define QD_SCREEN_HEIGHT    512
#define QD_BYTES_PER_ROW    (QD_SCREEN_WIDTH / 2)  // 2ピクセル/バイト

// カラー定義（16色パレット）
#define QD_COLOR_BLACK      0
#define QD_COLOR_BLUE       1
#define QD_COLOR_GREEN      2
#define QD_COLOR_CYAN       3
#define QD_COLOR_RED        4
#define QD_COLOR_MAGENTA    5
#define QD_COLOR_BROWN      6
#define QD_COLOR_WHITE      7
#define QD_COLOR_GRAY       8
#define QD_COLOR_LTBLUE     9
#define QD_COLOR_LTGREEN    10
#define QD_COLOR_LTCYAN     11
#define QD_COLOR_LTRED      12
#define QD_COLOR_LTMAGENTA  13
#define QD_COLOR_YELLOW     14
#define QD_COLOR_BRIGHT_WHITE 15

// フォント定義
#define QD_FONT_WIDTH       8
#define QD_FONT_HEIGHT      16
#define QD_FONT_DATA_ADDR   0xf3a800  // X68000標準フォントアドレス

// クリッピング矩形
typedef struct {
    int16_t x, y, width, height;
} QD_Rect;

// 初期化・基本制御
void qd_init(void);
void qd_set_clip_rect(int16_t x, int16_t y, uint16_t width, uint16_t height);
QD_Rect* qd_get_clip_rect(void);
bool qd_clip_point(int16_t x, int16_t y);
bool qd_clip_rect(int16_t* x, int16_t* y, uint16_t* width, uint16_t* height);

// ピクセル操作
void qd_set_pixel(int16_t x, int16_t y, uint8_t color);
uint8_t qd_get_pixel(int16_t x, int16_t y);
void qd_set_pixel_fast(int16_t x, int16_t y, uint8_t color);  // 最適化版

// 矩形・線描画
void qd_draw_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);
void qd_fill_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);
void qd_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color);
void qd_draw_hline(int16_t x, int16_t y, uint16_t length, uint8_t color);
void qd_draw_vline(int16_t x, int16_t y, uint16_t length, uint8_t color);

// 画面操作
void qd_clear_screen(uint8_t color);
void qd_clear_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);

// 文字・テキスト描画
void qd_draw_char(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color);
void qd_draw_text(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color);
int qd_draw_text_smart(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color, char* prev_str);

// パレット操作
void qd_set_palette(uint8_t index, uint16_t rgb);
uint16_t qd_get_palette(uint8_t index);
void qd_init_default_palette(void);

// 高速描画関数（68000最適化）
void qd_fill_rect_fast(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);
void qd_draw_rect_fast(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);

// VRAMバッファ管理
void qd_set_vram_buffer(uint16_t* buffer);
uint16_t* qd_get_vram_buffer(void);

// システム状態
bool qd_is_initialized(void);
uint16_t qd_get_screen_width(void);
uint16_t qd_get_screen_height(void);

// ユーティリティ
uint16_t qd_rgb_to_palette_index(uint8_t r, uint8_t g, uint8_t b);
int qd_clip_line(int16_t* x1, int16_t* y1, int16_t* x2, int16_t* y2);

// デバッグ・診断
void qd_print_info(void);
uint32_t qd_get_vram_size(void);
void qd_benchmark_pixel_access(void);
void qd_benchmark_rect_fill(void);