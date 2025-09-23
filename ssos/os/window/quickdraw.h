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
#include "ss_config.h"

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

// 描画モード定義
typedef enum {
    QD_MODE_COPY = 0,     // 通常描画（上書き）
    QD_MODE_XOR = 1,      // XOR描画
    QD_MODE_OR = 2,       // OR描画
    QD_MODE_AND = 3,      // AND描画
    QD_MODE_NOT = 4       // NOT描画
} QD_DrawMode;

// フォントスタイル定義
typedef enum {
    QD_FONT_NORMAL = 0,   // 通常フォント
    QD_FONT_BOLD = 1,     // ボールド
    QD_FONT_ITALIC = 2,   // イタリック
    QD_FONT_UNDERLINE = 4 // 下線
} QD_FontStyle;

// フォントサイズ定義
typedef enum {
    QD_FONT_SIZE_8x16 = 0,   // 8x16ピクセル
    QD_FONT_SIZE_16x32 = 1,  // 16x32ピクセル（倍率2）
    QD_FONT_SIZE_12x24 = 2,  // 12x24ピクセル（倍率1.5）
    QD_FONT_SIZE_6x12 = 3    // 6x12ピクセル（倍率0.75）
} QD_FontSize;

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

// 円・楕円描画
void qd_draw_circle(int16_t cx, int16_t cy, uint16_t radius, uint8_t color);
void qd_fill_circle(int16_t cx, int16_t cy, uint16_t radius, uint8_t color);
void qd_draw_ellipse(int16_t cx, int16_t cy, uint16_t rx, uint16_t ry, uint8_t color);
void qd_fill_ellipse(int16_t cx, int16_t cy, uint16_t rx, uint16_t ry, uint8_t color);

// 多角形描画
void qd_draw_triangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t color);
void qd_fill_triangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t color);
void qd_draw_polygon(const int16_t* points, uint16_t point_count, uint8_t color);
void qd_fill_polygon(const int16_t* points, uint16_t point_count, uint8_t color);

// 描画モード
void qd_set_draw_mode(QD_DrawMode mode);
QD_DrawMode qd_get_draw_mode(void);
void qd_set_pixel_mode(int16_t x, int16_t y, uint8_t color);
uint8_t qd_get_pixel_mode(int16_t x, int16_t y);

// フォント・テキスト描画（拡張）
void qd_set_font_size(QD_FontSize size);
void qd_set_font_style(QD_FontStyle style);
QD_FontSize qd_get_font_size(void);
QD_FontStyle qd_get_font_style(void);
void qd_draw_char_scaled(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color, float scale_x, float scale_y);
void qd_draw_text_scaled(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color, float scale_x, float scale_y);
void qd_draw_char_rotated(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color, float angle);
void qd_draw_text_rotated(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color, float angle);

// パフォーマンス最適化
void qd_enable_dma_mode(bool enable);
bool qd_is_dma_mode_enabled(void);
void qd_set_pixel_block(int16_t x, int16_t y, uint16_t width, uint16_t height, const uint8_t* pattern);
void qd_copy_rect(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y, uint16_t width, uint16_t height);
void qd_enable_cache(bool enable);
void qd_clear_cache(void);
void qd_set_render_buffer(uint16_t* buffer, uint32_t size);
void qd_flush_render_buffer(void);

// 画面操作
void qd_clear_screen(uint8_t color);
void qd_clear_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color);

// 文字・テキスト描画
void qd_draw_char(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color);
void qd_draw_text(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color);
int qd_draw_text_smart(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color, char* prev_str);

// パレット操作（基本）
void qd_set_palette(uint8_t index, uint16_t rgb);
uint16_t qd_get_palette(uint8_t index);
void qd_init_default_palette(void);

// パレット操作（拡張）
void qd_set_palette_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void qd_get_palette_rgb(uint8_t index, uint8_t* r, uint8_t* g, uint8_t* b);
void qd_copy_palette(uint8_t* dest_palette);
void qd_paste_palette(const uint8_t* src_palette);
void qd_fade_palette(uint8_t index, uint16_t target_rgb, uint8_t steps, uint16_t delay_ms);
void qd_animate_palette(uint8_t start_idx, uint8_t end_idx, const uint16_t* colors, uint8_t frame_count, uint16_t delay_ms);

// カラー変換ユーティリティ
uint16_t qd_rgb_to_grb(uint8_t r, uint8_t g, uint8_t b);
void qd_grb_to_rgb(uint16_t grb, uint8_t* r, uint8_t* g, uint8_t* b);
uint8_t qd_blend_colors(uint8_t color1, uint8_t color2, uint8_t ratio);
uint16_t qd_interpolate_color(uint16_t color1, uint16_t color2, uint8_t ratio);

// グラデーション機能
void qd_draw_gradient_h(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                       uint8_t start_color, uint8_t end_color);
void qd_draw_gradient_v(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                       uint8_t start_color, uint8_t end_color);
void qd_draw_gradient_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                          uint8_t top_left, uint8_t top_right, uint8_t bottom_left, uint8_t bottom_right);

// パレットバンク管理
void qd_set_palette_bank(uint8_t bank_index);
uint8_t qd_get_palette_bank(void);
void qd_save_palette_bank(uint8_t bank_index);
void qd_restore_palette_bank(uint8_t bank_index);

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

// ============================================================================
// レイヤー互換機能（フェーズ4: 既存レイヤーシステムとの統合）
// ============================================================================

// ソフトウェアレイヤー構造体（QuickDraw互換レイヤー）
typedef struct {
    uint16_t x, y, z;              // 位置とZオーダー
    uint16_t width, height;        // サイズ
    uint16_t attr;                 // 属性（表示状態など）
    uint8_t* buffer;              // ソフトウェアバッファ
    bool visible;                  // 表示状態
    bool dirty;                    // ダーティフラグ
    QD_Rect dirty_rect;           // ダーティ矩形
} QD_Layer;

// レイヤーマネージャー構造体
typedef struct {
    QD_Layer layers[MAX_LAYERS];   // レイヤー配列
    int16_t top_layer_idx;        // 最上位レイヤーインデックス
    QD_Layer* z_layers[MAX_LAYERS]; // Zオーダーソート済みレイヤー
    bool initialized;             // 初期化状態
} QD_LayerMgr;

// レイヤー属性定義（既存システムとの互換性）
#define QD_LAYER_ATTR_USED      0x01
#define QD_LAYER_ATTR_VISIBLE   0x02

// 最大レイヤー数
#ifndef MAX_LAYERS
#define MAX_LAYERS 16
#endif


// グローバル変数
extern QD_LayerMgr* qd_layer_mgr;

// レイヤー互換API関数
void qd_layer_init(void);
QD_Layer* qd_layer_create(void);
void qd_layer_destroy(QD_Layer* layer);
void qd_layer_set_position(QD_Layer* layer, int16_t x, int16_t y);
void qd_layer_set_size(QD_Layer* layer, uint16_t width, uint16_t height);
void qd_layer_set_z_order(QD_Layer* layer, int16_t z);
void qd_layer_show(QD_Layer* layer);
void qd_layer_hide(QD_Layer* layer);
void qd_layer_set_visible(QD_Layer* layer, bool visible);
void qd_layer_invalidate(QD_Layer* layer);
void qd_layer_invalidate_rect(QD_Layer* layer, int16_t x, int16_t y, uint16_t width, uint16_t height);
void qd_layer_blit_to_screen(QD_Layer* layer);
void qd_layer_blit_all_to_screen(void);
void qd_layer_blit_dirty_to_screen(void);

// ソフトウェアバッファ操作
uint8_t* qd_layer_get_buffer(QD_Layer* layer, int16_t x, int16_t y);
void qd_layer_clear_buffer(QD_Layer* layer, uint8_t color);
void qd_layer_copy_to_buffer(QD_Layer* layer, const uint8_t* src, uint16_t src_width, uint16_t src_height);

// 移行支援機能
void qd_enable_compatibility_mode(bool enable);
bool qd_is_compatibility_mode(void);

// 既存Layer APIとの完全互換関数（ラッパー）
QD_Layer* qd_layer_get(void);
void qd_layer_set(QD_Layer* layer, uint8_t* buffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void qd_layer_set_z(QD_Layer* layer, uint16_t z);
void qd_all_layer_draw(void);
void qd_all_layer_draw_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void qd_layer_draw_dirty_only(void);

// ============================================================================
// 既存Layer APIとの完全互換レイヤー（マクロ定義）
// ============================================================================

// 既存のlayer.h APIとの互換性を確保するためのマクロ定義
#define Layer QD_Layer
#define LayerMgr QD_LayerMgr

// 既存API関数名との互換性マクロ
#define ss_layer_mgr qd_layer_mgr
#define ss_layer_init() qd_layer_init()
#define ss_layer_get() qd_layer_get()
#define ss_layer_set(layer, vram, x, y, w, h) qd_layer_set(layer, vram, x, y, w, h)
#define ss_layer_set_z(layer, z) qd_layer_set_z(layer, z)
#define ss_all_layer_draw() qd_all_layer_draw()
#define ss_all_layer_draw_rect(x0, y0, x1, y1) qd_all_layer_draw_rect(x0, y0, x1, y1)
#define ss_layer_draw_dirty_only() qd_layer_draw_dirty_only()
#define ss_layer_invalidate(layer) qd_layer_invalidate(layer)
#define ss_layer_mark_dirty(layer, x, y, w, h) qd_layer_invalidate_rect(layer, x, y, w, h)
#define ss_layer_mark_clean(layer) // 簡易実装のため省略

// 互換性のために必要な追加マクロ
#define LAYER_ATTR_USED QD_LAYER_ATTR_USED
#define LAYER_ATTR_VISIBLE QD_LAYER_ATTR_VISIBLE