/**
 * @file quickdraw.c
 * @brief QuickDrawスタイルグラフィックスシステム実装
 *
 * 68000 16MHzに最適化された直接VRAM描画システム。
 * 16色カラーで高速なグラフィックス処理を提供。
 */

#include "quickdraw.h"
#include "kernel.h"
#include "vram.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// グローバル変数
static QD_Rect s_clip_rect = {0, 0, QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT};
static volatile uint16_t* s_vram_base = (uint16_t*)0x00c00000;
static QD_DrawMode s_current_draw_mode = QD_MODE_COPY;
static QD_FontSize s_current_font_size = QD_FONT_SIZE_8x16;
static QD_FontStyle s_current_font_style = QD_FONT_NORMAL;

// パフォーマンス最適化関連
static bool s_dma_mode_enabled = false;
static bool s_cache_enabled = false;
static uint16_t* s_render_buffer = NULL;
static uint32_t s_render_buffer_size = 0;
static uint32_t s_render_buffer_offset = 0;

#ifdef LOCAL_MODE
// LOCAL_MODEではソフトウェアバッファを使用（ワードアライメント保証）
static uint16_t s_local_vram[(QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2] __attribute__((aligned(4)));
#endif

// 互換性モードフラグ（レイヤーシステムとの互換性用）
static bool s_compatibility_mode = false;

// ソフトウェアレイヤー管理システム
static QD_Layer s_qd_layers[MAX_LAYERS];
static QD_Layer* s_z_layers[MAX_LAYERS];
static QD_LayerMgr s_qd_layer_mgr = {
    .layers = {0},
    .z_layers = {NULL},
    .top_layer_idx = 0,
    .initialized = false
};
QD_LayerMgr* qd_layer_mgr = &s_qd_layer_mgr;

// 16色パレット（GRBフォーマット、X68000互換）
static uint16_t s_palette[16] = {
    0x0000, // 0: Black
    0x0A80, // 1: Blue
    0x0500, // 2: Green
    0x0F80, // 3: Cyan
    0xA000, // 4: Red
    0xA500, // 5: Magenta
    0xA300, // 6: Brown
    0xFFF0, // 7: White
    0x5550, // 8: Gray
    0x5FF0, // 9: Light Blue
    0x5F50, // 10: Light Green
    0x5FF0, // 11: Light Cyan
    0xF550, // 12: Light Red
    0xF5F0, // 13: Light Magenta
    0xFF50, // 14: Yellow
    0xFFF0  // 15: Bright White
};

/**
 * @brief QuickDrawシステムを初期化
 *
 * VRAMベースアドレスを設定し、デフォルトパレットを初期化。
 * クリッピング矩形を画面全体に設定。
 */
void qd_init(void) {
    // VRAMベースアドレスの設定（LOCAL_MODE対応）
#ifdef LOCAL_MODE
    // LOCAL_MODEではデフォルトでソフトウェアバッファを使用
    if (s_vram_base == (uint16_t*)0x00c00000) {
        s_vram_base = s_local_vram;
    }
    // ソフトウェアバッファをゼロクリア
    for (uint32_t i = 0; i < sizeof(s_local_vram) / sizeof(uint16_t); i++) {
        s_local_vram[i] = 0;
    }
#else
    s_vram_base = (uint16_t*)0x00c00000;
#endif

    // クリッピング矩形を画面全体に設定
    s_clip_rect.x = 0;
    s_clip_rect.y = 0;
    s_clip_rect.width = QD_SCREEN_WIDTH;
    s_clip_rect.height = QD_SCREEN_HEIGHT;

    // デフォルトパレットの初期化
    qd_init_default_palette();

    // 互換性モードではレイヤーシステムも初期化
    if (s_compatibility_mode) {
        qd_layer_init();
    }
}

/**
 * @brief クリッピング矩形を設定
 *
 * @param x クリッピング矩形の左上X座標
 * @param y クリッピング矩形の左上Y座標
 * @param width クリッピング矩形の幅
 * @param height クリッピング矩形の高さ
 */
void qd_set_clip_rect(int16_t x, int16_t y, uint16_t width, uint16_t height) {
    s_clip_rect.x = x;
    s_clip_rect.y = y;
    s_clip_rect.width = width;
    s_clip_rect.height = height;

    // 画面境界内にクリッピング
    if (s_clip_rect.x < 0) s_clip_rect.x = 0;
    if (s_clip_rect.y < 0) s_clip_rect.y = 0;
    if (s_clip_rect.x + s_clip_rect.width > QD_SCREEN_WIDTH) {
        s_clip_rect.width = QD_SCREEN_WIDTH - s_clip_rect.x;
    }
    if (s_clip_rect.y + s_clip_rect.height > QD_SCREEN_HEIGHT) {
        s_clip_rect.height = QD_SCREEN_HEIGHT - s_clip_rect.y;
    }
}

/**
 * @brief 現在のクリッピング矩形を取得
 *
 * @return QD_Rect構造体へのポインタ
 */
QD_Rect* qd_get_clip_rect(void) {
    return &s_clip_rect;
}

/**
 * @brief 点をクリッピング矩形内かどうかを判定
 *
 * @param x X座標
 * @param y Y座標
 * @return 矩形内ならtrue
 */
bool qd_clip_point(int16_t x, int16_t y) {
    return (x >= s_clip_rect.x && x < s_clip_rect.x + s_clip_rect.width &&
            y >= s_clip_rect.y && y < s_clip_rect.y + s_clip_rect.height);
}

/**
 * @brief 矩形をクリッピング矩形内に収まるように調整
 *
 * @param x 矩形の左上X座標（調整される）
 * @param y 矩形の左上Y座標（調整される）
 * @param width 矩形の幅（調整される）
 * @param height 矩形の高さ（調整される）
 * @return 矩形がクリッピング矩形と重なる場合はtrue
 */
bool qd_clip_rect(int16_t* x, int16_t* y, uint16_t* width, uint16_t* height) {
    int16_t x1 = *x;
    int16_t y1 = *y;
    int16_t x2 = x1 + *width - 1;
    int16_t y2 = y1 + *height - 1;

    // クリッピング矩形との交差を計算
    int16_t cx1 = s_clip_rect.x;
    int16_t cy1 = s_clip_rect.y;
    int16_t cx2 = cx1 + s_clip_rect.width - 1;
    int16_t cy2 = cy1 + s_clip_rect.height - 1;

    // クリッピング
    if (x1 < cx1) x1 = cx1;
    if (y1 < cy1) y1 = cy1;
    if (x2 > cx2) x2 = cx2;
    if (y2 > cy2) y2 = cy2;

    // 結果が無効な場合はfalse
    if (x1 > x2 || y1 > y2) {
        return false;
    }

    // 調整された値を設定
    *x = x1;
    *y = y1;
    *width = x2 - x1 + 1;
    *height = y2 - y1 + 1;

    return true;
}

/**
 * @brief ピクセルを描画（クリッピングなし）
 *
 * @param x X座標
 * @param y Y座標
 * @param color カラーインデックス（0-15）
 */
void qd_set_pixel(int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || x >= QD_SCREEN_WIDTH || y < 0 || y >= QD_SCREEN_HEIGHT) {
        return;
    }

    // 2ピクセル/バイトのVRAMレイアウト
    uint32_t offset = (y * QD_SCREEN_WIDTH + x) / 2;

    // 両モードで境界チェックを強化
    uint32_t max_offset = (QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2;
    if (offset >= max_offset) {
        return;
    }

    // 68000アドレスエラーを防ぐための完全なチェック
    volatile uint16_t* vram_word_ptr = s_vram_base + offset;

    // ポインタがNULLまたはアライメント違反の可能性がある場合は安全に処理
    if (!vram_word_ptr || (uintptr_t)vram_word_ptr % 2 != 0) {
        return;
    }

    // ワードアライメントチェック（68000のアドレスエラー防止）
    uintptr_t vram_start = (uintptr_t)s_vram_base;
    uintptr_t vram_end = vram_start + max_offset * 2;
    uintptr_t word_addr = (uintptr_t)vram_word_ptr;

    if (word_addr < vram_start || word_addr >= vram_end || word_addr % 2 != 0) {
        return;
    }

    volatile uint16_t* vram_word = vram_word_ptr;

    // デバッグ情報（テスト時のみ）
#ifdef NATIVE_TEST
    printf("DEBUG qd_set_pixel(%d,%d, %d): offset=%d, x&1=%d\n",
           x, y, color, offset, x & 1);
#endif

    // ピクセル位置に応じて上位4ビットまたは下位4ビットを設定
    if (x & 1) {
        // 奇数ピクセル: 上位4ビット
        uint16_t old_value = *vram_word;
        uint16_t new_value = (old_value & 0xFFF0) | ((color & 0x0F) << 4);
        *vram_word = new_value;
#ifdef NATIVE_TEST
        printf("  Odd pixel: 0x%04X -> 0x%04X (set upper 4 bits to %d)\n",
               old_value, new_value, color & 0x0F);
#endif
    } else {
        // 偶数ピクセル: 下位4ビット
        uint16_t old_value = *vram_word;
        uint16_t new_value = (old_value & 0xFFF0) | (color & 0x0F);
        *vram_word = new_value;
#ifdef NATIVE_TEST
        printf("  Even pixel: 0x%04X -> 0x%04X (set lower 4 bits to %d)\n",
               old_value, new_value, color & 0x0F);
#endif
    }
}

/**
 * @brief ピクセルを取得
 *
 * @param x X座標
 * @param y Y座標
 * @return カラーインデックス（0-15）
 */
uint8_t qd_get_pixel(int16_t x, int16_t y) {
    if (x < 0 || x >= QD_SCREEN_WIDTH || y < 0 || y >= QD_SCREEN_HEIGHT) {
        return 0;
    }

    // 2ピクセル/バイトのVRAMレイアウト
    uint32_t offset = (y * QD_SCREEN_WIDTH + x) / 2;

    // 両モードで境界チェックを強化
    uint32_t max_offset = (QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2;
    if (offset >= max_offset) {
        return 0;
    }

    // 68000アドレスエラーを防ぐための完全なチェック
    volatile uint16_t* vram_word_ptr = s_vram_base + offset;

    // ポインタがNULLまたはアライメント違反の可能性がある場合は安全に処理
    if (!vram_word_ptr || (uintptr_t)vram_word_ptr % 2 != 0) {
        return 0;
    }

    // ワードアライメントチェック（68000のアドレスエラー防止）
    uintptr_t vram_start = (uintptr_t)s_vram_base;
    uintptr_t vram_end = vram_start + max_offset * 2;
    uintptr_t word_addr = (uintptr_t)vram_word_ptr;

    if (word_addr < vram_start || word_addr >= vram_end || word_addr % 2 != 0) {
        return 0;
    }

    uint16_t vram_word = *vram_word_ptr;

    // デバッグ情報（テスト時のみ）
#ifdef NATIVE_TEST
    printf("DEBUG qd_get_pixel(%d,%d): offset=%d, vram_word=0x%04X, x&1=%d\n",
           x, y, offset, vram_word, x & 1);
#endif

    // ピクセル位置に応じて上位4ビットまたは下位4ビットを抽出
    if (x & 1) {
        // 奇数ピクセル: 上位4ビット
        uint8_t result = (vram_word >> 4) & 0x0F;
#ifdef NATIVE_TEST
        printf("  Odd pixel: (0x%04X >> 4) & 0x0F = 0x%04X & 0x0F = %d\n",
               vram_word, vram_word >> 4, result);
#endif
        return result;
    } else {
        // 偶数ピクセル: 下位4ビット
        uint8_t result = vram_word & 0x0F;
#ifdef NATIVE_TEST
        printf("  Even pixel: 0x%04X & 0x0F = %d\n", vram_word, result);
#endif
        return result;
    }
}

/**
 * @brief 高速ピクセル描画（最適化版）
 *
 * @param x X座標
 * @param y Y座標
 * @param color カラーインデックス（0-15）
 */
void qd_set_pixel_fast(int16_t x, int16_t y, uint8_t color) {
    // クリッピングチェック
    if (!qd_clip_point(x, y)) {
        return;
    }

    // 直接VRAMアクセス（アライメント考慮）
    uint32_t offset = (y * QD_SCREEN_WIDTH + x) / 2;

    // 両モードで境界チェックを強化
    uint32_t max_offset = (QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2;
    if (offset >= max_offset) {
        return;
    }

    // 68000アドレスエラーを防ぐための完全なチェック
    volatile uint16_t* vram_word_ptr = s_vram_base + offset;

    // ポインタがNULLまたはアライメント違反の可能性がある場合は安全に処理
    if (!vram_word_ptr || (uintptr_t)vram_word_ptr % 2 != 0) {
        return;
    }

    // ワードアライメントチェック（68000のアドレスエラー防止）
    uintptr_t vram_start = (uintptr_t)s_vram_base;
    uintptr_t vram_end = vram_start + max_offset * 2;
    uintptr_t word_addr = (uintptr_t)vram_word_ptr;

    if (word_addr < vram_start || word_addr >= vram_end || word_addr % 2 != 0) {
        return;
    }

    volatile uint16_t* vram_word = vram_word_ptr;

#ifndef LOCAL_MODE
    // ハードウェアモードではVRAMアドレス範囲チェックを強化
    // 68000アドレスエラー防止のため、より安全なチェック
    if (vram_start == 0x00c00000) {
        uintptr_t hw_vram_end = vram_start + ((QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2) * 2;

        // アドレス範囲チェック（ワードアライメント含む）
        if (word_addr < vram_start || word_addr >= hw_vram_end || word_addr % 2 != 0) {
            return; // 範囲外またはアライメント違反
        }
    } else {
        // VRAMアドレスが正しく設定されていない場合は安全に処理
        return;
    }
#endif

    // ピクセル位置に応じて上位4ビットまたは下位4ビットを設定
    if (x & 1) {
        // 奇数ピクセル: 上位4ビット
        *vram_word = (*vram_word & 0xFFF0) | ((color & 0x0F) << 4);
    } else {
        // 偶数ピクセル: 下位4ビット
        *vram_word = (*vram_word & 0xFFF0) | (color & 0x0F);
    }
}

/**
 * @brief 画面全体をクリア
 *
 * @param color クリア色（0-15）
 */
void qd_clear_screen(uint8_t color) {
    uint16_t fill_word = (color << 4) | color;  // 2ピクセル分のデータ

    // 画面全体を高速クリア（768x512ピクセル）
    uint32_t total_words = (QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2;

    // 両モードで安全なサイズを使用
    uint32_t safe_words = total_words;
#ifdef LOCAL_MODE
    uint32_t local_max_words = sizeof(s_local_vram) / sizeof(uint16_t);
    if (safe_words > local_max_words) {
        safe_words = local_max_words;
    }
#endif

    // 68000アドレスエラーを防ぐための完全なチェック
    uintptr_t vram_start = (uintptr_t)s_vram_base;
    uintptr_t vram_end = vram_start + safe_words * 2;

    // 境界チェック付きでクリア
    for (uint32_t i = 0; i < safe_words; i++) {
        volatile uint16_t* dst_ptr = s_vram_base + i;

        // ポインタがNULLまたはアライメント違反の可能性がある場合はスキップ
        if (!dst_ptr || (uintptr_t)dst_ptr % 2 != 0) {
            continue;
        }

        // ワードアライメントチェック（68000のアドレスエラー防止）
        uintptr_t dst_addr = (uintptr_t)dst_ptr;
        if (dst_addr < vram_start || dst_addr >= vram_end || dst_addr % 2 != 0) {
            continue;
        }

        *dst_ptr = fill_word;
    }
}

/**
 * @brief 矩形領域をクリア
 *
 * @param x 矩形の左上X座標
 * @param y 矩形の左上Y座標
 * @param width 矩形の幅
 * @param height 矩形の高さ
 * @param color クリア色（0-15）
 */
void qd_clear_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    // クリッピング適用
    if (!qd_clip_rect(&x, &y, &width, &height)) {
        return;
    }

    qd_fill_rect_fast(x, y, width, height, color);
}

/**
 * @brief 矩形を描画（枠線のみ）
 *
 * @param x 矩形の左上X座標
 * @param y 矩形の左上Y座標
 * @param width 矩形の幅
 * @param height 矩形の高さ
 * @param color 描画色（0-15）
 */
void qd_draw_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    // クリッピング適用
    int16_t x1 = x, y1 = y;
    uint16_t w1 = width, h1 = height;
    if (!qd_clip_rect(&x1, &y1, &w1, &h1)) {
        return;
    }

    // 矩形の4辺を描画（枠線のみ）
    // 上辺
    qd_draw_hline(x1, y1, w1, color);

    // 下辺
    if (h1 > 1) {
        qd_draw_hline(x1, y1 + h1 - 1, w1, color);
    }

    // 左辺
    if (h1 > 2) {
        qd_draw_vline(x1, y1 + 1, h1 - 2, color);
    }

    // 右辺
    if (h1 > 2 && w1 > 1) {
        qd_draw_vline(x1 + w1 - 1, y1 + 1, h1 - 2, color);
    }
}

/**
 * @brief 矩形を塗りつぶし
 *
 * @param x 矩形の左上X座標
 * @param y 矩形の左上Y座標
 * @param width 矩形の幅
 * @param height 矩形の高さ
 * @param color 塗りつぶし色（0-15）
 */
void qd_fill_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    // クリッピング適用
    int16_t x1 = x, y1 = y;
    uint16_t w1 = width, h1 = height;
    if (!qd_clip_rect(&x1, &y1, &w1, &h1)) {
        return;
    }

    qd_fill_rect_fast(x1, y1, w1, h1, color);
}

/**
 * @brief 矩形塗りつぶし（高速最適化版）
 *
 * @param x 矩形の左上X座標
 * @param y 矩形の左上Y座標
 * @param width 矩形の幅
 * @param height 矩形の高さ
 * @param color 塗りつぶし色（0-15）
 */
void qd_fill_rect_fast(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    uint16_t fill_word = (color << 4) | color;  // 2ピクセル分のデータ

    for (uint16_t dy = 0; dy < height; dy++) {
        // 開始ピクセルインデックスを計算
        uint32_t start_pixel = (y + dy) * QD_SCREEN_WIDTH + x;
        uint32_t base_offset = start_pixel / 2;
        uint32_t max_offset = (QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2;

        // 境界チェック
        if (base_offset >= max_offset) {
            continue;
        }

        // 68000アドレスエラーを防ぐための完全なチェック
        volatile uint16_t* base_ptr = s_vram_base + base_offset;

        // ポインタがNULLまたはアライメント違反の可能性がある場合はスキップ
        if (!base_ptr || (uintptr_t)base_ptr % 2 != 0) {
            continue;
        }

        // ワードアライメントチェック（68000のアドレスエラー防止）
        uintptr_t vram_start = (uintptr_t)s_vram_base;
        uintptr_t vram_end = vram_start + max_offset * 2;
        uintptr_t base_addr = (uintptr_t)base_ptr;

        if (base_addr < vram_start || base_addr >= vram_end || base_addr % 2 != 0) {
            continue;
        }

        volatile uint16_t* dst = base_ptr;
        uint16_t words = width / 2;

        // ワード単位で高速塗りつぶし（境界チェック付き）
        for (uint16_t dx = 0; dx < words; dx++) {
            uint32_t current_offset = base_offset + dx;
            if (current_offset < max_offset) {
                // 各ポインタもチェック
                volatile uint16_t* word_ptr = dst + dx;
                if ((uintptr_t)word_ptr >= vram_start && (uintptr_t)word_ptr < vram_end) {
                    *word_ptr = fill_word;
                }
            }
        }

        // 端数の処理（奇数幅の場合）
        if (width & 1) {
            int16_t end_x = x + width - 1;
            if (end_x >= 0 && end_x < QD_SCREEN_WIDTH) {
                qd_set_pixel_fast(end_x, y + dy, color);
            }
        }
    }
}

/**
 * @brief 線を描画（簡易実装）
 *
 * @param x1 始点X座標
 * @param y1 始点Y座標
 * @param x2 終点X座標
 * @param y2 終点Y座標
 * @param color 描画色（0-15）
 */
void qd_draw_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
    // クリッピング適用
    if (!qd_clip_line(&x1, &y1, &x2, &y2)) {
        return;
    }

    // 簡易線描画（ブレゼンハムアルゴリズムの簡易版）
    int16_t dx = abs(x2 - x1);
    int16_t dy = abs(y2 - y1);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        qd_set_pixel_fast(x1, y1, color);

        if (x1 == x2 && y1 == y2) break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/**
 * @brief 文字を描画
 *
 * @param x 描画位置X座標
 * @param y 描画位置Y座標
 * @param c 描画する文字
 * @param fg_color 前景色（0-15）
 * @param bg_color 背景色（0-15）
 */
void qd_draw_char(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color) {
    // 8x16フォントデータへのアクセス
    volatile uint8_t* font_base = (uint8_t*)QD_FONT_DATA_ADDR;
    volatile uint8_t* font_char = font_base + (c * QD_FONT_HEIGHT);

    // フォントデータ範囲チェック
    uintptr_t font_start = (uintptr_t)font_base;
    if ((uintptr_t)font_char >= font_start + 256 * QD_FONT_HEIGHT) {
        return; // 無効な文字コード
    }

    for (uint16_t row = 0; row < QD_FONT_HEIGHT; row++) {
        uint8_t font_byte = font_char[row];
        for (uint8_t col = 0; col < QD_FONT_WIDTH; col++) {
            uint8_t color = (font_byte & (0x80 >> col)) ? fg_color : bg_color;
            qd_set_pixel(x + col, y + row, color);
        }
    }
}

/**
 * @brief テキストを描画
 *
 * @param x 描画位置X座標
 * @param y 描画位置Y座標
 * @param str 描画する文字列
 * @param fg_color 前景色（0-15）
 * @param bg_color 背景色（0-15）
 */
void qd_draw_text(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color) {
    if (!str) return;

    int16_t curr_x = x;
    for (const char* p = str; *p; p++) {
        qd_draw_char(curr_x, y, *p, fg_color, bg_color);
        curr_x += QD_FONT_WIDTH;
    }
}

/**
 * @brief スマートテキスト描画（変更時のみ描画）
 *
 * @param x 描画位置X座標
 * @param y 描画位置Y座標
 * @param str 新しい文字列
 * @param fg_color 前景色（0-15）
 * @param bg_color 背景色（0-15）
 * @param prev_str 前の文字列（比較用）
 * @return 描画が行われた場合は1、変更なしの場合は0
 */
int qd_draw_text_smart(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color, char* prev_str) {
    if (!str) return 0;

    // 文字列比較（簡易版）
    if (prev_str) {
        const char* p1 = str;
        const char* p2 = prev_str;
        while (*p1 && *p2) {
            if (*p1 != *p2) break;
            p1++;
            p2++;
        }
        if (*p1 == *p2) {
            return 0;  // 変更なし
        }
    }

    // 古いテキスト領域をクリア
    if (prev_str) {
        int old_len = 0;
        while (prev_str[old_len]) old_len++;
        qd_clear_rect(x, y, old_len * QD_FONT_WIDTH, QD_FONT_HEIGHT, bg_color);
    }

    // 新しいテキストを描画
    qd_draw_text(x, y, str, fg_color, bg_color);

    return 1;  // 描画実行
}

/**
 * @brief 線のクリッピング
 *
 * @param x1 線始点X座標（調整される）
 * @param y1 線始点Y座標（調整される）
 * @param x2 線終点X座標（調整される）
 * @param y2 線終点Y座標（調整される）
 * @return クリッピング矩形と重なる場合はtrue
 */
int qd_clip_line(int16_t* x1, int16_t* y1, int16_t* x2, int16_t* y2) {
    // 簡易クリッピング実装
    // 完全な線クリッピングは複雑なので、両端点が矩形内にある場合のみ描画
    if (qd_clip_point(*x1, *y1) && qd_clip_point(*x2, *y2)) {
        return 1;
    }
    return 0;
}

/**
 * @brief パレットカラーを設定
 *
 * @param index パレットインデックス（0-15）
 * @param rgb RGB値（GRBフォーマット）
 */
void qd_set_palette(uint8_t index, uint16_t rgb) {
    if (index < 16) {
        s_palette[index] = rgb;
    }
}

/**
 * @brief パレットカラーを取得
 *
 * @param index パレットインデックス（0-15）
 * @return RGB値（GRBフォーマット）
 */
uint16_t qd_get_palette(uint8_t index) {
    if (index < 16) {
        return s_palette[index];
    }
    return 0;
}

/**
 * @brief デフォルトパレットを初期化
 *
 * X68000標準の16色パレットを設定。
 * 注意: 実際のハードウェアパレット設定はブート時に行われるため、
 * ここではソフトウェアパレットのみ設定。
 */
void qd_init_default_palette(void) {
    // ソフトウェアパレット設定（ハードウェア設定はブート時）
    // 実際のハードウェアパレット設定はss_init_palette()で行われる
    // ここでは内部パレット配列のみ設定

    // 標準16色パレット（GRBフォーマット）
    s_palette[0] = 0x0000;     // Black
    s_palette[1] = 0x0A80;     // Blue
    s_palette[2] = 0x0500;     // Green
    s_palette[3] = 0x0F80;     // Cyan
    s_palette[4] = 0xA000;     // Red
    s_palette[5] = 0xA500;     // Magenta
    s_palette[6] = 0xA300;     // Brown
    s_palette[7] = 0xFFF0;     // White
    s_palette[8] = 0x5550;     // Gray
    s_palette[9] = 0x5FF0;     // Light Blue
    s_palette[10] = 0x5F50;    // Light Green
    s_palette[11] = 0x5FF0;    // Light Cyan
    s_palette[12] = 0xF550;    // Light Red
    s_palette[13] = 0xF5F0;    // Light Magenta
    s_palette[14] = 0xFF50;    // Yellow
    s_palette[15] = 0xFFF0;    // Bright White
}

/**
 * @brief QuickDrawシステムを最小構成で初期化（デバッグ用）
 *
 * ソフトウェアバッファ初期化を除外したバージョン。
 * アドレスエラー調査用。
 */
void qd_init_minimal(void) {
    // VRAMベースアドレスの設定（LOCAL_MODE対応）
#ifdef LOCAL_MODE
    // LOCAL_MODEではデフォルトでソフトウェアバッファを使用
    if (s_vram_base == (uint16_t*)0x00c00000) {
        s_vram_base = s_local_vram;
    }
    // ソフトウェアバッファの初期化は行わない（アドレスエラー調査用）
#else
    s_vram_base = (uint16_t*)0x00c00000;
#endif

    // クリッピング矩形を画面全体に設定
    s_clip_rect.x = 0;
    s_clip_rect.y = 0;
    s_clip_rect.width = QD_SCREEN_WIDTH;
    s_clip_rect.height = QD_SCREEN_HEIGHT;

    // デフォルトパレットの初期化は行わない（アドレスエラー調査用）
    // qd_init_default_palette();
}

/**
 * @brief RGB値をパレットインデックスに変換
 *
 * @param r 赤成分（0-255）
 * @param g 緑成分（0-255）
 * @param b 青成分（0-255）
 * @return 最も近いパレットインデックス
 */
uint16_t qd_rgb_to_palette_index(uint8_t r, uint8_t g, uint8_t b) {
    // 簡易色距離計算（実際にはより洗練されたアルゴリズムを使用）
    static const uint8_t palette_rgb[16][3] = {
        {0, 0, 0},      // Black
        {0, 0, 170},    // Blue
        {0, 170, 0},    // Green
        {0, 170, 170},  // Cyan
        {170, 0, 0},    // Red
        {170, 0, 170},  // Magenta
        {170, 85, 0},   // Brown
        {170, 170, 170}, // White
        {85, 85, 85},   // Gray
        {85, 85, 255},  // Light Blue
        {85, 255, 85},  // Light Green
        {85, 255, 255}, // Light Cyan
        {255, 85, 85},  // Light Red
        {255, 85, 255}, // Light Magenta
        {255, 255, 85}, // Yellow
        {255, 255, 255} // Bright White
    };

    uint16_t best_index = 0;
    uint32_t min_distance = ~0;

    for (uint16_t i = 0; i < 16; i++) {
        uint32_t dr = r - palette_rgb[i][0];
        uint32_t dg = g - palette_rgb[i][1];
        uint32_t db = b - palette_rgb[i][2];
        uint32_t distance = dr*dr + dg*dg + db*db;

        if (distance < min_distance) {
            min_distance = distance;
            best_index = i;
        }
    }

    return best_index;
}

/**
 * @brief システム情報を表示（デバッグ用）
 */
void qd_print_info(void) {
    // デバッグ情報表示（実際には適切な出力関数を使用）
    // printf("QuickDraw Graphics System\n");
    // printf("Screen: %dx%d, 16 colors\n", QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT);
    // printf("VRAM: %p - %p\n", s_vram_base, s_vram_base + (QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2);
    // printf("Clip Rect: (%d,%d)-(%d,%d)\n",
    //        s_clip_rect.x, s_clip_rect.y,
    //        s_clip_rect.x + s_clip_rect.width, s_clip_rect.y + s_clip_rect.height);
}

/**
 * @brief VRAMサイズを取得
 *
 * @return VRAMサイズ（バイト単位）
 */
uint32_t qd_get_vram_size(void) {
#ifdef LOCAL_MODE
    return sizeof(s_local_vram);
#else
    return (QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2;
#endif
}

/**
 * @brief ピクセルアクセス速度をベンチマーク（デバッグ用）
 */
void qd_benchmark_pixel_access(void) {
    // ベンチマーク機能（実際には適切なタイマー関数を使用）
    uint32_t pixel_count = 1000;

    for (uint32_t i = 0; i < pixel_count; i++) {
        qd_set_pixel_fast(i % QD_SCREEN_WIDTH, i % QD_SCREEN_HEIGHT, i % 16);
    }

    // printf("Pixel access benchmark: %d pixels in %d ticks\n", pixel_count, end_time - start_time);
}

/**
 * @brief 矩形塗りつぶし速度をベンチマーク（デバッグ用）
 */
void qd_benchmark_rect_fill(void) {
    // ベンチマーク機能（実際には適切なタイマー関数を使用）
    uint32_t rect_count = 100;

    for (uint32_t i = 0; i < rect_count; i++) {
        qd_fill_rect_fast(10 + i, 10 + i, 50, 30, (i % 15) + 1);
    }

    // printf("Rect fill benchmark: %d rects in %d ticks\n", rect_count, end_time - start_time);
}

/**
 * @brief VRAMバッファを設定
 *
 * @param buffer 新しいVRAMバッファへのポインタ
 */
void qd_set_vram_buffer(uint16_t* buffer) {
    if (buffer) {
#ifdef LOCAL_MODE
        // LOCAL_MODEではカスタムバッファを設定可能
        s_vram_base = buffer;
#else
        // ハードウェアモードではVRAMアドレスのみ許可
        if (buffer == (uint16_t*)0x00c00000) {
            s_vram_base = buffer;
        }
#endif
    }
}

/**
 * @brief 現在のVRAMバッファを取得
 *
 * @return VRAMバッファへのポインタ
 */
uint16_t* qd_get_vram_buffer(void) {
    return (uint16_t*)s_vram_base;
}

/**
 * @brief QuickDrawシステムが初期化されているかを確認
 *
 * @return 初期化済みの場合はtrue
 */
bool qd_is_initialized(void) {
    return (s_vram_base != NULL);
}

/**
 * @brief 画面幅を取得
 *
 * @return 画面幅（ピクセル単位）
 */
uint16_t qd_get_screen_width(void) {
    return QD_SCREEN_WIDTH;
}

/**
 * @brief 画面高さを取得
 *
 * @return 画面高さ（ピクセル単位）
 */
uint16_t qd_get_screen_height(void) {
    return QD_SCREEN_HEIGHT;
}

/**
 * @brief 水平線を描画
 *
 * @param x 線の開始X座標
 * @param y Y座標
 * @param length 線の長さ
 * @param color 描画色（0-15）
 */
void qd_draw_hline(int16_t x, int16_t y, uint16_t length, uint8_t color) {
    for (uint16_t i = 0; i < length; i++) {
        qd_set_pixel_fast(x + i, y, color);
    }
}

/**
 * @brief 垂直線を描画
 *
 * @param x X座標
 * @param y 線の開始Y座標
 * @param length 線の長さ
 * @param color 描画色（0-15）
 */
void qd_draw_vline(int16_t x, int16_t y, uint16_t length, uint8_t color) {
    for (uint16_t i = 0; i < length; i++) {
        qd_set_pixel_fast(x, y + i, color);
    }
}

// ============================================================================
// レイヤー互換機能の実装（フェーズ4: 既存レイヤーシステムとの統合）
// ============================================================================

/**
 * @brief 互換性モードを有効/無効にする
 *
 * @param enable trueで有効、falseで無効
 */
void qd_enable_compatibility_mode(bool enable) {
    s_compatibility_mode = enable;
    if (enable && qd_is_initialized() && !qd_layer_mgr->initialized) {
        qd_layer_init();
    }
}

/**
 * @brief 互換性モードの状態を取得
 *
 * @return 互換性モードが有効ならtrue
 */
bool qd_is_compatibility_mode(void) {
    return s_compatibility_mode;
}

/**
 * @brief ソフトウェアレイヤーシステムを初期化
 */
void qd_layer_init(void) {
    if (qd_layer_mgr->initialized) {
        return; // 既に初期化済み
    }

    // すべてのレイヤーを未使用状態に初期化
    for (int i = 0; i < MAX_LAYERS; i++) {
        qd_layer_mgr->layers[i].buffer = NULL;
        qd_layer_mgr->layers[i].attr = 0;
        qd_layer_mgr->layers[i].visible = false;
        qd_layer_mgr->layers[i].dirty = false;
        qd_layer_mgr->z_layers[i] = NULL;
    }

    qd_layer_mgr->top_layer_idx = 0;
    qd_layer_mgr->initialized = true;
}

/**
 * @brief 新しいレイヤーを作成
 *
 * @return 作成されたレイヤーへのポインタ、失敗時はNULL
 */
QD_Layer* qd_layer_create(void) {
    if (!qd_layer_mgr->initialized) {
        qd_layer_init();
    }

    // 未使用のレイヤーを探す
    for (int i = 0; i < MAX_LAYERS; i++) {
        if ((qd_layer_mgr->layers[i].attr & QD_LAYER_ATTR_USED) == 0) {
            QD_Layer* layer = &qd_layer_mgr->layers[i];
            layer->attr = QD_LAYER_ATTR_USED;
            layer->visible = true;
            layer->dirty = true;
            layer->z = qd_layer_mgr->top_layer_idx;
            layer->x = 0;
            layer->y = 0;
            layer->width = 0;
            layer->height = 0;
            layer->buffer = NULL;

            // Zオーダーリストに追加
            qd_layer_mgr->z_layers[qd_layer_mgr->top_layer_idx] = layer;
            qd_layer_mgr->top_layer_idx++;

            return layer;
        }
    }

    return NULL; // 使用可能なレイヤーがない
}

/**
 * @brief レイヤーを破壊
 *
 * @param layer 破壊するレイヤー
 */
void qd_layer_destroy(QD_Layer* layer) {
    if (!layer || (layer->attr & QD_LAYER_ATTR_USED) == 0) {
        return;
    }

    // メモリ解放
    if (layer->buffer) {
        free(layer->buffer);
        layer->buffer = NULL;
    }

    // レイヤーを未使用状態に
    layer->attr = 0;
    layer->visible = false;
    layer->dirty = false;

    // Zオーダーリストから除去
    for (int i = 0; i < qd_layer_mgr->top_layer_idx; i++) {
        if (qd_layer_mgr->z_layers[i] == layer) {
            // 後続のレイヤーをシフト
            for (int j = i; j < qd_layer_mgr->top_layer_idx - 1; j++) {
                qd_layer_mgr->z_layers[j] = qd_layer_mgr->z_layers[j + 1];
                qd_layer_mgr->z_layers[j]->z--;
            }
            qd_layer_mgr->top_layer_idx--;
            break;
        }
    }
}

// ============================================================================
// フェーズ2: 多角形描画機能
// ============================================================================

/**
 * @brief 三角形を描画
 *
 * @param x1 点1 X座標
 * @param y1 点1 Y座標
 * @param x2 点2 X座標
 * @param y2 点2 Y座標
 * @param x3 点3 X座標
 * @param y3 点3 Y座標
 * @param color 描画色
 */
void qd_draw_triangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t color) {
    // 3辺を描画
    qd_draw_line(x1, y1, x2, y2, color);
    qd_draw_line(x2, y2, x3, y3, color);
    qd_draw_line(x3, y3, x1, y1, color);
}

/**
 * @brief 三角形を塗りつぶし（簡易版）
 *
 * @param x1 点1 X座標
 * @param y1 点1 Y座標
 * @param x2 点2 X座標
 * @param y2 点2 Y座標
 * @param x3 点3 X座標
 * @param y3 点3 Y座標
 * @param color 塗りつぶし色
 */
void qd_fill_triangle(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint8_t color) {
    // バウンディングボックスを計算
    int16_t min_x = x1 < x2 ? (x1 < x3 ? x1 : x3) : (x2 < x3 ? x2 : x3);
    int16_t max_x = x1 > x2 ? (x1 > x3 ? x1 : x3) : (x2 > x3 ? x2 : x3);
    int16_t min_y = y1 < y2 ? (y1 < y3 ? y1 : y3) : (y2 < y3 ? y2 : y3);
    int16_t max_y = y1 > y2 ? (y1 > y3 ? y1 : y3) : (y2 > y3 ? y2 : y3);

    // クリッピング適用
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > QD_SCREEN_WIDTH) max_x = QD_SCREEN_WIDTH;
    if (max_y > QD_SCREEN_HEIGHT) max_y = QD_SCREEN_HEIGHT;

    // 各ピクセルについて三角形内にあるかチェック
    for (int16_t y = min_y; y < max_y; y++) {
        for (int16_t x = min_x; x < max_x; x++) {
            // 点が三角形内にあるか判定（重心座標法）
            int32_t denom = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3);

            if (denom == 0) continue; // 面積が0

            // 重心座標の計算
            int32_t alpha = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) * 255 / denom;
            int32_t beta = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) * 255 / denom;
            int32_t gamma = 255 - alpha - beta;

            // すべての重心座標が正の場合は三角形内
            if (alpha >= 0 && beta >= 0 && gamma >= 0) {
                qd_set_pixel_fast(x, y, color);
            }
        }
    }
}

// ============================================================================
// フェーズ2: フォント・テキスト描画機能の改善
// ============================================================================

/**
 * @brief フォントサイズを設定
 *
 * @param size フォントサイズ
 */
void qd_set_font_size(QD_FontSize size) {
    s_current_font_size = size;
}

/**
 * @brief フォントスタイルを設定
 *
 * @param style フォントスタイル
 */
void qd_set_font_style(QD_FontStyle style) {
    s_current_font_style = style;
}

/**
 * @brief 現在のフォントサイズを取得
 *
 * @return 現在のフォントサイズ
 */
QD_FontSize qd_get_font_size(void) {
    return s_current_font_size;
}

/**
 * @brief 現在のフォントスタイルを取得
 *
 * @return 現在のフォントスタイル
 */
QD_FontStyle qd_get_font_style(void) {
    return s_current_font_style;
}

/**
 * @brief フォントサイズに応じた幅を取得
 *
 * @return フォント幅（ピクセル）
 */
static uint8_t qd_get_font_width(void) {
    switch (s_current_font_size) {
        case QD_FONT_SIZE_6x12: return 6;
        case QD_FONT_SIZE_8x16: return 8;
        case QD_FONT_SIZE_12x24: return 12;
        case QD_FONT_SIZE_16x32: return 16;
        default: return 8;
    }
}

/**
 * @brief フォントサイズに応じた高さを取得
 *
 * @return フォント高さ（ピクセル）
 */
static uint8_t qd_get_font_height(void) {
    switch (s_current_font_size) {
        case QD_FONT_SIZE_6x12: return 12;
        case QD_FONT_SIZE_8x16: return 16;
        case QD_FONT_SIZE_12x24: return 24;
        case QD_FONT_SIZE_16x32: return 32;
        default: return 16;
    }
}

/**
 * @brief 文字をスケーリングして描画
 *
 * @param x 描画位置X座標
 * @param y 描画位置Y座標
 * @param c 描画する文字
 * @param fg_color 前景色
 * @param bg_color 背景色
 * @param scale_x X方向スケール
 * @param scale_y Y方向スケール
 */
void qd_draw_char_scaled(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color, float scale_x, float scale_y) {
    uint8_t font_w = qd_get_font_width();
    uint8_t font_h = qd_get_font_height();

    // 8x16フォントデータへのアクセス
    volatile uint8_t* font_base = (uint8_t*)QD_FONT_DATA_ADDR;
    volatile uint8_t* font_char = font_base + (c * QD_FONT_HEIGHT);

    // フォントデータ範囲チェック
    uintptr_t font_start = (uintptr_t)font_base;
    if ((uintptr_t)font_char >= font_start + 256 * QD_FONT_HEIGHT) {
        return; // 無効な文字コード
    }

    // スケーリング描画
    for (uint16_t sy = 0; sy < font_h; sy++) {
        uint8_t font_byte = font_char[sy];
        for (uint8_t sx = 0; sx < font_w; sx++) {
            uint8_t color = (font_byte & (0x80 >> sx)) ? fg_color : bg_color;

            // スケーリングして描画
            for (float dy = 0; dy < scale_y; dy++) {
                for (float dx = 0; dx < scale_x; dx++) {
                    int16_t draw_x = x + (int16_t)(sx * scale_x + dx);
                    int16_t draw_y = y + (int16_t)(sy * scale_y + dy);

                    if (draw_x >= 0 && draw_x < QD_SCREEN_WIDTH &&
                        draw_y >= 0 && draw_y < QD_SCREEN_HEIGHT) {
                        qd_set_pixel(draw_x, draw_y, color);
                    }
                }
            }
        }
    }
}

/**
 * @brief テキストをスケーリングして描画
 *
 * @param x 描画位置X座標
 * @param y 描画位置Y座標
 * @param str 描画する文字列
 * @param fg_color 前景色
 * @param bg_color 背景色
 * @param scale_x X方向スケール
 * @param scale_y Y方向スケール
 */
void qd_draw_text_scaled(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color, float scale_x, float scale_y) {
    if (!str) return;

    int16_t curr_x = x;
    uint8_t font_w = qd_get_font_width();

    for (const char* p = str; *p; p++) {
        qd_draw_char_scaled(curr_x, y, *p, fg_color, bg_color, scale_x, scale_y);
        curr_x += (int16_t)(font_w * scale_x);
    }
}

/**
 * @brief 文字を回転して描画
 *
 * @param x 描画位置X座標
 * @param y 描画位置Y座標
 * @param c 描画する文字
 * @param fg_color 前景色
 * @param bg_color 背景色
 * @param angle 回転角度（度）
 */

// 内部フォント回転関数宣言
static void qd_draw_char_rotated_90(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color);
static void qd_draw_char_rotated_180(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color);
static void qd_draw_char_rotated_270(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color);
static void qd_draw_text_rotated_90(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color);
static void qd_draw_text_rotated_180(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color);
static void qd_draw_text_rotated_270(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color);
static bool qd_point_in_polygon(int16_t x, int16_t y, const int16_t* points, uint16_t point_count);

void qd_draw_char_rotated(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color, float angle) {
    // 簡易回転実装（90度単位のみ）
    if (angle == 90.0f) {
        qd_draw_char_rotated_90(x, y, c, fg_color, bg_color);
    } else if (angle == 180.0f) {
        qd_draw_char_rotated_180(x, y, c, fg_color, bg_color);
    } else if (angle == 270.0f) {
        qd_draw_char_rotated_270(x, y, c, fg_color, bg_color);
    } else {
        // 0度または未対応角度の場合は通常描画
        qd_draw_char(x, y, c, fg_color, bg_color);
    }
}

/**
 * @brief テキストを回転して描画
 *
 * @param x 描画位置X座標
 * @param y 描画位置Y座標
 * @param str 描画する文字列
 * @param fg_color 前景色
 * @param bg_color 背景色
 * @param angle 回転角度（度）
 */
void qd_draw_text_rotated(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color, float angle) {
    if (!str) return;

    // 簡易回転実装（90度単位のみ）
    if (angle == 90.0f) {
        qd_draw_text_rotated_90(x, y, str, fg_color, bg_color);
    } else if (angle == 180.0f) {
        qd_draw_text_rotated_180(x, y, str, fg_color, bg_color);
    } else if (angle == 270.0f) {
        qd_draw_text_rotated_270(x, y, str, fg_color, bg_color);
    } else {
        // 0度または未対応角度の場合は通常描画
        qd_draw_text(x, y, str, fg_color, bg_color);
    }
}

// ============================================================================
// 内部ユーティリティ関数
// ============================================================================

static inline int16_t min(int16_t a, int16_t b) {
    return a < b ? a : b;
}

static inline int16_t max(int16_t a, int16_t b) {
    return a > b ? a : b;
}

// ============================================================================
// 内部フォント回転関数
// ============================================================================

/**
 * @brief 文字を90度回転して描画
 */
static void qd_draw_char_rotated_90(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color) {
    volatile uint8_t* font_base = (uint8_t*)QD_FONT_DATA_ADDR;
    volatile uint8_t* font_char = font_base + (c * QD_FONT_HEIGHT);

    if ((uintptr_t)font_char >= (uintptr_t)font_base + 256 * QD_FONT_HEIGHT) {
        return;
    }

    // 90度回転：(x,y) -> (y, -x)
    for (uint8_t row = 0; row < QD_FONT_HEIGHT; row++) {
        uint8_t font_byte = font_char[row];
        for (uint8_t col = 0; col < QD_FONT_WIDTH; col++) {
            uint8_t color = (font_byte & (0x80 >> col)) ? fg_color : bg_color;
            qd_set_pixel(x + QD_FONT_HEIGHT - 1 - row, y + col, color);
        }
    }
}

/**
 * @brief 文字を180度回転して描画
 */
static void qd_draw_char_rotated_180(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color) {
    volatile uint8_t* font_base = (uint8_t*)QD_FONT_DATA_ADDR;
    volatile uint8_t* font_char = font_base + (c * QD_FONT_HEIGHT);

    if ((uintptr_t)font_char >= (uintptr_t)font_base + 256 * QD_FONT_HEIGHT) {
        return;
    }

    // 180度回転：(x,y) -> (-x, -y)
    for (uint8_t row = 0; row < QD_FONT_HEIGHT; row++) {
        uint8_t font_byte = font_char[row];
        for (uint8_t col = 0; col < QD_FONT_WIDTH; col++) {
            uint8_t color = (font_byte & (0x80 >> col)) ? fg_color : bg_color;
            qd_set_pixel(x + QD_FONT_WIDTH - 1 - col, y + QD_FONT_HEIGHT - 1 - row, color);
        }
    }
}

/**
 * @brief 文字を270度回転して描画
 */
static void qd_draw_char_rotated_270(int16_t x, int16_t y, char c, uint8_t fg_color, uint8_t bg_color) {
    volatile uint8_t* font_base = (uint8_t*)QD_FONT_DATA_ADDR;
    volatile uint8_t* font_char = font_base + (c * QD_FONT_HEIGHT);

    if ((uintptr_t)font_char >= (uintptr_t)font_base + 256 * QD_FONT_HEIGHT) {
        return;
    }

    // 270度回転：(x,y) -> (-y, x)
    for (uint8_t row = 0; row < QD_FONT_HEIGHT; row++) {
        uint8_t font_byte = font_char[row];
        for (uint8_t col = 0; col < QD_FONT_WIDTH; col++) {
            uint8_t color = (font_byte & (0x80 >> col)) ? fg_color : bg_color;
            qd_set_pixel(x + row, y + QD_FONT_WIDTH - 1 - col, color);
        }
    }
}

/**
 * @brief テキストを90度回転して描画
 */
static void qd_draw_text_rotated_90(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color) {
    if (!str) return;

    int16_t curr_y = y;
    for (const char* p = str; *p; p++) {
        qd_draw_char_rotated_90(x, curr_y, *p, fg_color, bg_color);
        curr_y += QD_FONT_WIDTH;
    }
}

/**
 * @brief テキストを180度回転して描画
 */
static void qd_draw_text_rotated_180(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color) {
    if (!str) return;

    int16_t curr_x = x;
    for (const char* p = str; *p; p++) {
        qd_draw_char_rotated_180(curr_x, y, *p, fg_color, bg_color);
        curr_x -= QD_FONT_WIDTH;
    }
}

/**
 * @brief テキストを270度回転して描画
 */
static void qd_draw_text_rotated_270(int16_t x, int16_t y, const char* str, uint8_t fg_color, uint8_t bg_color) {
    if (!str) return;

    int16_t curr_y = y;
    for (const char* p = str; *p; p++) {
        qd_draw_char_rotated_270(x, curr_y, *p, fg_color, bg_color);
        curr_y -= QD_FONT_WIDTH;
    }
}

// ============================================================================
// フェーズ2: パフォーマンス最適化
// ============================================================================

/**
 * @brief DMAモードを有効/無効にする
 *
 * @param enable trueで有効、falseで無効
 */
void qd_enable_dma_mode(bool enable) {
    s_dma_mode_enabled = enable;
}

/**
 * @brief DMAモードが有効かどうかを確認
 *
 * @return DMAモードが有効ならtrue
 */
bool qd_is_dma_mode_enabled(void) {
    return s_dma_mode_enabled;
}

/**
 * @brief ブロック単位でピクセルを設定
 *
 * @param x 開始X座標
 * @param y 開始Y座標
 * @param width 幅
 * @param height 高さ
 * @param pattern パターンデータ
 */
void qd_set_pixel_block(int16_t x, int16_t y, uint16_t width, uint16_t height, const uint8_t* pattern) {
    if (!pattern) return;

    for (uint16_t dy = 0; dy < height; dy++) {
        for (uint16_t dx = 0; dx < width; dx++) {
            uint8_t color = pattern[dy * width + dx];
            qd_set_pixel_fast(x + dx, y + dy, color);
        }
    }
}

/**
 * @brief 矩形領域をコピー
 *
 * @param src_x ソースX座標
 * @param src_y ソースY座標
 * @param dst_x デスティネーションX座標
 * @param dst_y デスティネーションY座標
 * @param width 幅
 * @param height 高さ
 */
void qd_copy_rect(int16_t src_x, int16_t src_y, int16_t dst_x, int16_t dst_y, uint16_t width, uint16_t height) {
    // 簡易実装：ピクセル単位でコピー
    for (uint16_t dy = 0; dy < height; dy++) {
        for (uint16_t dx = 0; dx < width; dx++) {
            uint8_t color = qd_get_pixel(src_x + dx, src_y + dy);
            qd_set_pixel_fast(dst_x + dx, dst_y + dy, color);
        }
    }
}

/**
 * @brief キャッシュを有効/無効にする
 *
 * @param enable trueで有効、falseで無効
 */
void qd_enable_cache(bool enable) {
    s_cache_enabled = enable;
    if (!enable) {
        qd_clear_cache();
    }
}

/**
 * @brief キャッシュをクリア
 */
void qd_clear_cache(void) {
    s_render_buffer_offset = 0;
}

/**
 * @brief レンダーバッファを設定
 *
 * @param buffer バッファへのポインタ
 * @param size バッファサイズ
 */
void qd_set_render_buffer(uint16_t* buffer, uint32_t size) {
    s_render_buffer = buffer;
    s_render_buffer_size = size;
    s_render_buffer_offset = 0;
}

/**
 * @brief レンダーバッファをフラッシュ（VRAMに転送）
 */
void qd_flush_render_buffer(void) {
    if (!s_render_buffer || s_render_buffer_offset == 0) {
        return;
    }

    // DMAモードが有効の場合はDMA転送をシミュレート
    if (s_dma_mode_enabled) {
        // 実際のDMA転送はハードウェア依存
        // ここでは単純なメモリコピーとして実装
        for (uint32_t i = 0; i < s_render_buffer_offset; i++) {
            // レンダーバッファの内容をVRAMに転送
            // 実際の実装ではDMAコントローラを使用
        }
    } else {
        // 通常のメモリコピー
        // 実際の実装では適切な転送関数を使用
    }

    s_render_buffer_offset = 0;
}

/**
 * @brief バッファ付きピクセル描画（パフォーマンス最適化版）
 *
 * @param x X座標
 * @param y Y座標
 * @param color カラーインデックス
 */
void qd_set_pixel_buffered(int16_t x, int16_t y, uint8_t color) {
    // キャッシュが有効でバッファが設定されている場合
    if (s_cache_enabled && s_render_buffer && s_render_buffer_offset < s_render_buffer_size) {
        // ピクセルデータをバッファに格納
        // 実際の実装では適切なデータ構造を使用
        s_render_buffer_offset++;

        // バッファが満杯になったらフラッシュ
        if (s_render_buffer_offset >= s_render_buffer_size) {
            qd_flush_render_buffer();
        }
    } else {
        // 直接描画
        qd_set_pixel_fast(x, y, color);
    }
}

/**
 * @brief メモリ使用量を最適化
 */
void qd_optimize_memory_usage(void) {
    // メモリ断片化を防ぐための処理
    // 実際の実装ではメモリプールやガベージコレクションを考慮
}

/**
 * @brief 描画パフォーマンス統計を取得
 *
 * @param pixels_drawn 描画されたピクセル数へのポインタ
 * @param cache_hits キャッシュヒット数へのポインタ
 */
void qd_get_performance_stats(uint32_t* pixels_drawn, uint32_t* cache_hits) {
    if (pixels_drawn) *pixels_drawn = 0; // 実際の実装ではカウンターを使用
    if (cache_hits) *cache_hits = 0;     // 実際の実装ではカウンターを使用
}

/**
 * @brief 高速メモリクリア
 *
 * @param x 開始X座標
 * @param y 開始Y座標
 * @param width 幅
 * @param height 高さ
 * @param color クリア色
 */
void qd_fast_clear_rect(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    uint16_t fill_word = (color << 4) | color;

    // ワード単位で高速クリア
    for (uint16_t dy = 0; dy < height; dy++) {
        uint32_t start_pixel = (y + dy) * QD_SCREEN_WIDTH + x;
        uint32_t base_offset = start_pixel / 2;
        uint32_t max_offset = (QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2;

        if (base_offset >= max_offset) continue;

        volatile uint16_t* base_ptr = s_vram_base + base_offset;

        // 境界チェック付きで高速クリア
        uint16_t words = width / 2;
        for (uint16_t dx = 0; dx < words; dx++) {
            uint32_t current_offset = base_offset + dx;
            if (current_offset < max_offset) {
                volatile uint16_t* word_ptr = base_ptr + dx;
                *word_ptr = fill_word;
            }
        }

        // 端数の処理
        if (width & 1) {
            int16_t end_x = x + width - 1;
            if (end_x >= 0 && end_x < QD_SCREEN_WIDTH) {
                qd_set_pixel_fast(end_x, y + dy, color);
            }
        }
    }
}

/**
 * @brief 多角形を描画
 *
 * @param points 頂点配列（x, yの交互）
 * @param point_count 頂点数
 * @param color 描画色
 */
void qd_draw_polygon(const int16_t* points, uint16_t point_count, uint8_t color) {
    if (!points || point_count < 3) return;

    // 各辺を描画
    for (uint16_t i = 0; i < point_count - 1; i++) {
        int16_t x1 = points[i * 2];
        int16_t y1 = points[i * 2 + 1];
        int16_t x2 = points[(i + 1) * 2];
        int16_t y2 = points[(i + 1) * 2 + 1];
        qd_draw_line(x1, y1, x2, y2, color);
    }

    // 最後の辺（最後の点から最初の点へ）
    int16_t x1 = points[(point_count - 1) * 2];
    int16_t y1 = points[(point_count - 1) * 2 + 1];
    int16_t x2 = points[0];
    int16_t y2 = points[1];
    qd_draw_line(x1, y1, x2, y2, color);
}

/**
 * @brief 多角形を塗りつぶし（簡易版）
 *
 * @param points 頂点配列（x, yの交互）
 * @param point_count 頂点数
 * @param color 塗りつぶし色
 */
void qd_fill_polygon(const int16_t* points, uint16_t point_count, uint8_t color) {
    if (!points || point_count < 3) return;

    // バウンディングボックスを計算
    int16_t min_x = QD_SCREEN_WIDTH;
    int16_t max_x = 0;
    int16_t min_y = QD_SCREEN_HEIGHT;
    int16_t max_y = 0;

    for (uint16_t i = 0; i < point_count; i++) {
        int16_t x = points[i * 2];
        int16_t y = points[i * 2 + 1];

        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }

    // クリッピング適用
    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x > QD_SCREEN_WIDTH) max_x = QD_SCREEN_WIDTH;
    if (max_y > QD_SCREEN_HEIGHT) max_y = QD_SCREEN_HEIGHT;

    // 各ピクセルについて多角形内にあるかチェック
    for (int16_t y = min_y; y < max_y; y++) {
        for (int16_t x = min_x; x < max_x; x++) {
            if (qd_point_in_polygon(x, y, points, point_count)) {
                qd_set_pixel_fast(x, y, color);
            }
        }
    }
}

/**
 * @brief 点が多角形内にあるか判定（レイキャスティング法）
 *
 * @param x X座標
 * @param y Y座標
 * @param points 頂点配列
 * @param point_count 頂点数
 * @return 多角形内ならtrue
 */
static bool qd_point_in_polygon(int16_t x, int16_t y, const int16_t* points, uint16_t point_count) {
    if (!points || point_count < 3) return false;

    int16_t inside = 0;
    int16_t xinters;
    int16_t p1x, p1y, p2x, p2y;

    p1x = points[(point_count - 1) * 2];
    p1y = points[(point_count - 1) * 2 + 1];

    for (uint16_t i = 0; i < point_count; i++) {
        p2x = points[i * 2];
        p2y = points[i * 2 + 1];

        if (y > min(p1y, p2y)) {
            if (y <= max(p1y, p2y)) {
                if (x <= max(p1x, p2x)) {
                    if (p1y != p2y) {
                        xinters = (y - p1y) * (p2x - p1x) / (p2y - p1y) + p1x;
                    }
                    if (p1x == p2x || x <= xinters) {
                        inside = !inside;
                    }
                }
            }
        }

        p1x = p2x;
        p1y = p2y;
    }

    return inside != 0;
}

// ユーティリティ関数

// ============================================================================
// フェーズ2: 描画モード機能
// ============================================================================

/**
 * @brief 描画モードを設定
 *
 * @param mode 描画モード
 */
void qd_set_draw_mode(QD_DrawMode mode) {
    s_current_draw_mode = mode;
}

/**
 * @brief 現在の描画モードを取得
 *
 * @return 現在の描画モード
 */
QD_DrawMode qd_get_draw_mode(void) {
    return s_current_draw_mode;
}

/**
 * @brief モードを考慮したピクセル描画
 *
 * @param x X座標
 * @param y Y座標
 * @param color 描画色
 */
void qd_set_pixel_mode(int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || x >= QD_SCREEN_WIDTH || y < 0 || y >= QD_SCREEN_HEIGHT) {
        return;
    }

    uint8_t current_color = qd_get_pixel(x, y);

    // 描画モードに応じて色を計算
    uint8_t final_color;
    switch (s_current_draw_mode) {
        case QD_MODE_COPY:
            final_color = color;
            break;
        case QD_MODE_XOR:
            final_color = current_color ^ color;
            break;
        case QD_MODE_OR:
            final_color = current_color | color;
            break;
        case QD_MODE_AND:
            final_color = current_color & color;
            break;
        case QD_MODE_NOT:
            final_color = ~color & 0x0F; // 4ビットマスク
            break;
        default:
            final_color = color;
            break;
    }

    qd_set_pixel(x, y, final_color);
}

/**
 * @brief モードを考慮したピクセル取得
 *
 * @param x X座標
 * @param y Y座標
 * @return ピクセル色
 */
uint8_t qd_get_pixel_mode(int16_t x, int16_t y) {
    return qd_get_pixel(x, y);
}

/**
 * @brief モード対応の矩形塗りつぶし
 *
 * @param x 矩形の左上X座標
 * @param y 矩形の左上Y座標
 * @param width 矩形の幅
 * @param height 矩形の高さ
 * @param color 塗りつぶし色
 */
void qd_fill_rect_mode(int16_t x, int16_t y, uint16_t width, uint16_t height, uint8_t color) {
    // クリッピング適用
    int16_t x1 = x, y1 = y;
    uint16_t w1 = width, h1 = height;
    if (!qd_clip_rect(&x1, &y1, &w1, &h1)) {
        return;
    }

    // 描画モードがCOPYの場合は通常の高速描画を使用
    if (s_current_draw_mode == QD_MODE_COPY) {
        qd_fill_rect_fast(x1, y1, w1, h1, color);
        return;
    }

    // モード対応の描画
    for (uint16_t dy = 0; dy < h1; dy++) {
        for (uint16_t dx = 0; dx < w1; dx++) {
            qd_set_pixel_mode(x1 + dx, y1 + dy, color);
        }
    }
}

/**
 * @brief モード対応の線描画
 *
 * @param x1 始点X座標
 * @param y1 始点Y座標
 * @param x2 終点X座標
 * @param y2 終点Y座標
 * @param color 描画色
 */
void qd_draw_line_mode(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
    // クリッピング適用
    int16_t cx1 = x1, cy1 = y1, cx2 = x2, cy2 = y2;
    if (!qd_clip_line(&cx1, &cy1, &cx2, &cy2)) {
        return;
    }

    // 描画モードがCOPYの場合は通常の線描画を使用
    if (s_current_draw_mode == QD_MODE_COPY) {
        qd_draw_line(cx1, cy1, cx2, cy2, color);
        return;
    }

    // 簡易線描画（ブレゼンハムアルゴリズム）
    int16_t dx = abs(cx2 - cx1);
    int16_t dy = abs(cy2 - cy1);
    int16_t sx = (cx1 < cx2) ? 1 : -1;
    int16_t sy = (cy1 < cy2) ? 1 : -1;
    int16_t err = dx - dy;

    int16_t x = cx1;
    int16_t y = cy1;

    while (1) {
        qd_set_pixel_mode(x, y, color);

        if (x == cx2 && y == cy2) break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

/**
 * @brief レイヤーの位置を設定
 *
 * @param layer 対象レイヤー
 * @param x X座標
 * @param y Y座標
 */
void qd_layer_set_position(QD_Layer* layer, int16_t x, int16_t y) {
    if (!layer) return;

    layer->x = x;
    layer->y = y;
    qd_layer_invalidate(layer);
}

/**
 * @brief レイヤーのサイズを設定
 *
 * @param layer 対象レイヤー
 * @param width 幅
 * @param height 高さ
 */
void qd_layer_set_size(QD_Layer* layer, uint16_t width, uint16_t height) {
    if (!layer) return;

    // サイズが変更された場合はバッファを再確保
    if (layer->width != width || layer->height != height) {
        // 既存バッファを解放
        if (layer->buffer) {
            free(layer->buffer);
        }

        // 新しいバッファを確保（16色モード: 2ピクセル/バイト）
        uint32_t buffer_size = (width * height) / 2;
        layer->buffer = (uint8_t*)malloc(buffer_size);
        if (layer->buffer) {
            // バッファをクリア
            memset(layer->buffer, 0, buffer_size);
        }
    }

    layer->width = width;
    layer->height = height;
    qd_layer_invalidate(layer);
}

/**
 * @brief レイヤーのZオーダーを設定
 *
 * @param layer 対象レイヤー
 * @param z Zオーダー値
 */
void qd_layer_set_z_order(QD_Layer* layer, int16_t z) {
    if (!layer || z < 0 || z >= MAX_LAYERS) {
        return;
    }

    // 現在の位置を除去
    for (int i = 0; i < qd_layer_mgr->top_layer_idx; i++) {
        if (qd_layer_mgr->z_layers[i] == layer) {
            // 後続のレイヤーをシフト
            for (int j = i; j < qd_layer_mgr->top_layer_idx - 1; j++) {
                qd_layer_mgr->z_layers[j] = qd_layer_mgr->z_layers[j + 1];
            }
            qd_layer_mgr->top_layer_idx--;
            break;
        }
    }

    // 新しい位置に挿入
    if (z >= qd_layer_mgr->top_layer_idx) {
        // 最後に追加
        qd_layer_mgr->z_layers[qd_layer_mgr->top_layer_idx] = layer;
        qd_layer_mgr->top_layer_idx++;
    } else {
        // 中間に挿入
        for (int i = qd_layer_mgr->top_layer_idx; i > z; i--) {
            qd_layer_mgr->z_layers[i] = qd_layer_mgr->z_layers[i - 1];
        }
        qd_layer_mgr->z_layers[z] = layer;
        qd_layer_mgr->top_layer_idx++;
    }

    layer->z = z;
}

/**
 * @brief レイヤーを表示状態に設定
 *
 * @param layer 対象レイヤー
 */
void qd_layer_show(QD_Layer* layer) {
    if (layer) {
        layer->visible = true;
        layer->attr |= QD_LAYER_ATTR_VISIBLE;
        qd_layer_invalidate(layer);
    }
}

/**
 * @brief レイヤーを非表示状態に設定
 *
 * @param layer 対象レイヤー
 */
void qd_layer_hide(QD_Layer* layer) {
    if (layer) {
        layer->visible = false;
        layer->attr &= ~QD_LAYER_ATTR_VISIBLE;
    }
}

/**
 * @brief レイヤーの表示状態を設定
 *
 * @param layer 対象レイヤー
 * @param visible 表示状態
 */
void qd_layer_set_visible(QD_Layer* layer, bool visible) {
    if (visible) {
        qd_layer_show(layer);
    } else {
        qd_layer_hide(layer);
    }
}

/**
 * @brief レイヤーを無効化（再描画が必要にマーク）
 *
 * @param layer 対象レイヤー
 */
void qd_layer_invalidate(QD_Layer* layer) {
    if (layer) {
        layer->dirty = true;
        layer->dirty_rect.x = 0;
        layer->dirty_rect.y = 0;
        layer->dirty_rect.width = layer->width;
        layer->dirty_rect.height = layer->height;
    }
}

/**
 * @brief レイヤーの矩形領域を無効化
 *
 * @param layer 対象レイヤー
 * @param x X座標
 * @param y Y座標
 * @param width 幅
 * @param height 高さ
 */
void qd_layer_invalidate_rect(QD_Layer* layer, int16_t x, int16_t y, uint16_t width, uint16_t height) {
    if (!layer || !layer->visible) return;

    // 範囲をレイヤー内に収める
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + width > layer->width) width = layer->width - x;
    if (y + height > layer->height) height = layer->height - y;

    if (width <= 0 || height <= 0) return;

    // ダーティ領域を統合
    if (layer->dirty) {
        int16_t x1 = layer->dirty_rect.x;
        int16_t y1 = layer->dirty_rect.y;
        int16_t x2 = x1 + layer->dirty_rect.width;
        int16_t y2 = y1 + layer->dirty_rect.height;

        int16_t new_x1 = (x < x1) ? x : x1;
        int16_t new_y1 = (y < y1) ? y : y1;
        int16_t new_x2 = ((x + width) > x2) ? (x + width) : x2;
        int16_t new_y2 = ((y + height) > y2) ? (y + height) : y2;

        layer->dirty_rect.x = new_x1;
        layer->dirty_rect.y = new_y1;
        layer->dirty_rect.width = new_x2 - new_x1;
        layer->dirty_rect.height = new_y2 - new_y1;
    } else {
        layer->dirty_rect.x = x;
        layer->dirty_rect.y = y;
        layer->dirty_rect.width = width;
        layer->dirty_rect.height = height;
        layer->dirty = true;
    }
}

/**
 * @brief レイヤーのバッファからピクセルを取得
 *
 * @param layer 対象レイヤー
 * @param x X座標（レイヤー内相対）
 * @param y Y座標（レイヤー内相対）
 * @return ピクセル値
 */
uint8_t* qd_layer_get_buffer(QD_Layer* layer, int16_t x, int16_t y) {
    if (!layer || !layer->buffer || x < 0 || y < 0 || x >= layer->width || y >= layer->height) {
        return NULL;
    }

    uint32_t offset = (y * layer->width + x) / 2;
    return &layer->buffer[offset];
}

/**
 * @brief レイヤーのバッファをクリア
 *
 * @param layer 対象レイヤー
 * @param color クリア色
 */
void qd_layer_clear_buffer(QD_Layer* layer, uint8_t color) {
    if (!layer || !layer->buffer) return;

    uint16_t fill_word = (color << 4) | color;
    uint32_t buffer_size = (layer->width * layer->height) / 2;

    for (uint32_t i = 0; i < buffer_size; i++) {
        layer->buffer[i] = fill_word;
    }
}

/**
 * @brief 外部バッファからレイヤーにデータをコピー
 *
 * @param layer 対象レイヤー
 * @param src ソースバッファ
 * @param src_width ソース幅
 * @param src_height ソース高さ
 */
void qd_layer_copy_to_buffer(QD_Layer* layer, const uint8_t* src, uint16_t src_width, uint16_t src_height) {
    if (!layer || !layer->buffer || !src) return;

    uint16_t copy_width = (src_width < layer->width) ? src_width : layer->width;
    uint16_t copy_height = (src_height < layer->height) ? src_height : layer->height;

    for (uint16_t y = 0; y < copy_height; y++) {
        for (uint16_t x = 0; x < copy_width; x++) {
            uint32_t src_offset = (y * src_width + x) / 2;
            uint32_t dst_offset = (y * layer->width + x) / 2;

            uint8_t src_word = src[src_offset];
            uint8_t dst_word = layer->buffer[dst_offset];

            // ピクセル位置に応じてコピー
            if ((x & 1) == 0) {
                // 偶数ピクセル: 下位4ビット
                dst_word = (dst_word & 0xF0) | (src_word & 0x0F);
            } else {
                // 奇数ピクセル: 上位4ビット
                dst_word = (dst_word & 0x0F) | ((src_word & 0x0F) << 4);
            }

            layer->buffer[dst_offset] = dst_word;
        }
    }

    qd_layer_invalidate_rect(layer, 0, 0, copy_width, copy_height);
}

/**
 * @brief レイヤーを画面に転送
 *
 * @param layer 転送するレイヤー
 */
void qd_layer_blit_to_screen(QD_Layer* layer) {
    if (!layer || !layer->visible || !layer->buffer || layer->width == 0 || layer->height == 0) {
        return;
    }

    // 画面座標に変換
    int16_t screen_x = layer->x;
    int16_t screen_y = layer->y;

    // 画面外チェック
    if (screen_x >= QD_SCREEN_WIDTH || screen_y >= QD_SCREEN_HEIGHT) {
        return;
    }

    // クリッピング
    int16_t start_x = (screen_x < 0) ? -screen_x : 0;
    int16_t start_y = (screen_y < 0) ? -screen_y : 0;
    int16_t end_x = (screen_x + layer->width > QD_SCREEN_WIDTH) ? QD_SCREEN_WIDTH - screen_x : layer->width;
    int16_t end_y = (screen_y + layer->height > QD_SCREEN_HEIGHT) ? QD_SCREEN_HEIGHT - screen_y : layer->height;

    if (start_x >= end_x || start_y >= end_y) {
        return;
    }

    // レイヤーから画面へ転送
    for (int16_t y = start_y; y < end_y; y++) {
        for (int16_t x = start_x; x < end_x; x++) {
            // レイヤーバッファからピクセル取得
            uint32_t layer_offset = (y * layer->width + x) / 2;
            uint8_t layer_word = layer->buffer[layer_offset];

            uint8_t color;
            if (x & 1) {
                // 奇数ピクセル: 上位4ビット
                color = (layer_word >> 4) & 0x0F;
            } else {
                // 偶数ピクセル: 下位4ビット
                color = layer_word & 0x0F;
            }

            // 画面に描画（透明色チェックは後で実装）
            qd_set_pixel_fast(screen_x + x, screen_y + y, color);
        }
    }
}

/**
 * @brief すべてのレイヤーを画面に転送
 */
void qd_layer_blit_all_to_screen(void) {
    if (!qd_layer_mgr->initialized) return;

    // Zオーダーに従って描画
    for (int i = 0; i < qd_layer_mgr->top_layer_idx; i++) {
        QD_Layer* layer = qd_layer_mgr->z_layers[i];
        if (layer && layer->visible) {
            qd_layer_blit_to_screen(layer);
        }
    }
}

/**
 * @brief ダーティ領域のみを画面に転送
 */
void qd_layer_blit_dirty_to_screen(void) {
    if (!qd_layer_mgr->initialized) return;

    // Zオーダーに従って描画
    for (int i = 0; i < qd_layer_mgr->top_layer_idx; i++) {
        QD_Layer* layer = qd_layer_mgr->z_layers[i];
        if (layer && layer->visible && layer->dirty) {
            qd_layer_blit_to_screen(layer);
            layer->dirty = false;
        }
    }
}

// ============================================================================
// 既存Layer APIとの完全互換関数（ラッパー関数）
// ============================================================================

/**
 * @brief 既存API互換: レイヤーを取得（QD_Layer*を返す）
 *
 * @return 作成されたレイヤーへのポインタ
 */
QD_Layer* qd_layer_get(void) {
    return qd_layer_create();
}

/**
 * @brief 既存API互換: レイヤーを設定
 *
 * @param layer 対象レイヤー
 * @param buffer バッファ（使用しない）
 * @param x X座標
 * @param y Y座標
 * @param width 幅
 * @param height 高さ
 */
void qd_layer_set(QD_Layer* layer, uint8_t* buffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    if (!layer) return;

    qd_layer_set_position(layer, x, y);
    qd_layer_set_size(layer, width, height);

    // バッファが提供された場合はコピー
    if (buffer) {
        qd_layer_copy_to_buffer(layer, buffer, width, height);
    }
}

/**
 * @brief 既存API互換: レイヤーのZオーダーを設定
 *
 * @param layer 対象レイヤー
 * @param z Zオーダー
 */
void qd_layer_set_z(QD_Layer* layer, uint16_t z) {
    if (layer) {
        qd_layer_set_z_order(layer, z);
    }
}

/**
 * @brief 既存API互換: 全レイヤーを描画
 */
void qd_all_layer_draw(void) {
    qd_layer_blit_all_to_screen();
}

/**
 * @brief 既存API互換: 矩形領域のレイヤーを描画
 *
 * @param x0 開始X座標
 * @param y0 開始Y座標
 * @param x1 終了X座標
 * @param y1 終了Y座標
 */
void qd_all_layer_draw_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // 簡易実装: 全レイヤーを描画
    qd_layer_blit_all_to_screen();
}

/**
 * @brief 既存API互換: ダーティ領域のみを描画
 */
void qd_layer_draw_dirty_only(void) {
    qd_layer_blit_dirty_to_screen();
}

// ============================================================================
// フェーズ2: パレット管理機能の強化
// ============================================================================

/**
 * @brief RGB値をパレットに設定
 *
 * @param index パレットインデックス（0-15）
 * @param r 赤成分（0-255）
 * @param g 緑成分（0-255）
 * @param b 青成分（0-255）
 */
void qd_set_palette_rgb(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= 16) return;

    // X68000のGRBフォーマットに変換（Green:Red:Blue）
    uint16_t grb = ((g & 0xF0) << 4) | (r & 0xF0) | ((b & 0xF0) >> 4);
    s_palette[index] = grb;
}

/**
 * @brief パレットからRGB値を取得
 *
 * @param index パレットインデックス（0-15）
 * @param r 赤成分へのポインタ
 * @param g 緑成分へのポインタ
 * @param b 青成分へのポインタ
 */
void qd_get_palette_rgb(uint8_t index, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (index >= 16 || !r || !g || !b) return;

    uint16_t grb = s_palette[index];
    *g = (grb >> 8) & 0xF0;  // Green
    *r = (grb >> 4) & 0xF0;   // Red
    *b = (grb << 4) & 0xF0;   // Blue
}

/**
 * @brief パレットをコピー
 *
 * @param dest_palette コピー先パレット配列（16要素）
 */
void qd_copy_palette(uint8_t* dest_palette) {
    if (!dest_palette) return;

    for (uint8_t i = 0; i < 16; i++) {
        dest_palette[i] = s_palette[i];
    }
}

/**
 * @brief パレットを貼り付け
 *
 * @param src_palette コピー元パレット配列（16要素）
 */
void qd_paste_palette(const uint8_t* src_palette) {
    if (!src_palette) return;

    for (uint8_t i = 0; i < 16; i++) {
        s_palette[i] = src_palette[i];
    }
}

/**
 * @brief パレットをフェード
 *
 * @param index パレットインデックス
 * @param target_rgb 目標RGB値（GRBフォーマット）
 * @param steps ステップ数
 * @param delay_ms 各ステップ間の遅延（ミリ秒）
 */
void qd_fade_palette(uint8_t index, uint16_t target_rgb, uint8_t steps, uint16_t delay_ms) {
    if (index >= 16 || steps == 0) return;

    uint16_t start_rgb = s_palette[index];
    int16_t delta = (int16_t)target_rgb - (int16_t)start_rgb;
    int16_t step_size = delta / (int16_t)steps;

    for (uint8_t i = 1; i <= steps; i++) {
        uint16_t new_rgb = (uint16_t)((int16_t)start_rgb + step_size * (int16_t)i);
        s_palette[index] = new_rgb;

        // 実際のハードウェアではここで適切な遅延関数を呼ぶ
        // delay_ms_microseconds(delay_ms * 1000);
    }

    s_palette[index] = target_rgb; // 最終値を正確に設定
}

/**
 * @brief パレットアニメーション
 *
 * @param start_idx 開始インデックス
 * @param end_idx 終了インデックス
 * @param colors カラー配列
 * @param frame_count フレーム数
 * @param delay_ms フレーム間隔（ミリ秒）
 */
void qd_animate_palette(uint8_t start_idx, uint8_t end_idx, const uint16_t* colors,
                       uint8_t frame_count, uint16_t delay_ms) {
    if (!colors || start_idx >= end_idx || end_idx > 16) return;

    uint8_t color_count = end_idx - start_idx;
    if (color_count > frame_count) return;

    for (uint8_t frame = 0; frame < frame_count; frame++) {
        // 各パレットインデックスに色を設定
        for (uint8_t i = 0; i < color_count; i++) {
            s_palette[start_idx + i] = colors[frame * color_count + i];
        }

        // 実際のハードウェアではここで適切な遅延関数を呼ぶ
        // delay_ms_microseconds(delay_ms * 1000);
    }
}

// ============================================================================
// カラー変換ユーティリティ
// ============================================================================

/**
 * @brief RGB値をGRBフォーマットに変換（X68000用）
 *
 * @param r 赤成分（0-255）
 * @param g 緑成分（0-255）
 * @param b 青成分（0-255）
 * @return GRBフォーマットの16ビット値
 */
uint16_t qd_rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    // X68000のGRBフォーマット: GGGG RRRR BBBB
    return ((g & 0xF0) << 4) | (r & 0xF0) | ((b & 0xF0) >> 4);
}

/**
 * @brief GRBフォーマットをRGB値に変換
 *
 * @param grb GRBフォーマットの16ビット値
 * @param r 赤成分へのポインタ
 * @param g 緑成分へのポインタ
 * @param b 青成分へのポインタ
 */
void qd_grb_to_rgb(uint16_t grb, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (!r || !g || !b) return;

    *g = (grb >> 8) & 0xF0;  // Green (上位8ビット)
    *r = (grb >> 4) & 0xF0;   // Red (中位4ビット)
    *b = (grb << 4) & 0xF0;   // Blue (下位4ビット)
}

/**
 * @brief 2つの色をブレンド
 *
 * @param color1 色1（0-15）
 * @param color2 色2（0-15）
 * @param ratio ブレンド率（0-255、0=完全にcolor1、255=完全にcolor2）
 * @return ブレンドされた色インデックス
 */
uint8_t qd_blend_colors(uint8_t color1, uint8_t color2, uint8_t ratio) {
    uint8_t r1, g1, b1, r2, g2, b2;

    qd_get_palette_rgb(color1, &r1, &g1, &b1);
    qd_get_palette_rgb(color2, &r2, &g2, &b2);

    uint8_t r = (r1 * (255 - ratio) + r2 * ratio) / 255;
    uint8_t g = (g1 * (255 - ratio) + g2 * ratio) / 255;
    uint8_t b = (b1 * (255 - ratio) + b2 * ratio) / 255;

    return qd_rgb_to_palette_index(r, g, b);
}

/**
 * @brief 色を線形補間
 *
 * @param color1 色1（GRBフォーマット）
 * @param color2 色2（GRBフォーマット）
 * @param ratio 補間率（0-255、0=完全にcolor1、255=完全にcolor2）
 * @return 補間された色（GRBフォーマット）
 */
uint16_t qd_interpolate_color(uint16_t color1, uint16_t color2, uint8_t ratio) {
    uint8_t r1, g1, b1, r2, g2, b2;

    qd_grb_to_rgb(color1, &r1, &g1, &b1);
    qd_grb_to_rgb(color2, &r2, &g2, &b2);

    uint8_t r = (r1 * (255 - ratio) + r2 * ratio) / 255;
    uint8_t g = (g1 * (255 - ratio) + g2 * ratio) / 255;
    uint8_t b = (b1 * (255 - ratio) + b2 * ratio) / 255;

    return qd_rgb_to_grb(r, g, b);
}

// ============================================================================
// グラデーション描画機能
// ============================================================================

/**
 * @brief 水平グラデーションを描画
 *
 * @param x X座標
 * @param y Y座標
 * @param width 幅
 * @param height 高さ
 * @param start_color 開始色
 * @param end_color 終了色
 */
void qd_draw_gradient_h(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                       uint8_t start_color, uint8_t end_color) {
    if (width == 0 || height == 0) return;

    for (uint16_t py = 0; py < height; py++) {
        for (uint16_t px = 0; px < width; px++) {
            uint8_t ratio = (px * 255) / (width - 1);
            uint8_t color = qd_blend_colors(start_color, end_color, ratio);
            qd_set_pixel_fast(x + px, y + py, color);
        }
    }
}

/**
 * @brief 垂直グラデーションを描画
 *
 * @param x X座標
 * @param y Y座標
 * @param width 幅
 * @param height 高さ
 * @param start_color 開始色
 * @param end_color 終了色
 */
void qd_draw_gradient_v(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                       uint8_t start_color, uint8_t end_color) {
    if (width == 0 || height == 0) return;

    for (uint16_t py = 0; py < height; py++) {
        uint8_t ratio = (py * 255) / (height - 1);
        uint8_t color = qd_blend_colors(start_color, end_color, ratio);

        // 水平線を高速描画
        for (uint16_t px = 0; px < width; px++) {
            qd_set_pixel_fast(x + px, y + py, color);
        }
    }
}

/**
 * @brief 4点グラデーション矩形を描画
 *
 * @param x X座標
 * @param y Y座標
 * @param width 幅
 * @param height 高さ
 * @param top_left 左上色
 * @param top_right 右上色
 * @param bottom_left 左下色
 * @param bottom_right 右下色
 */
void qd_draw_gradient_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                          uint8_t top_left, uint8_t top_right, uint8_t bottom_left, uint8_t bottom_right) {
    if (width == 0 || height == 0) return;

    for (uint16_t py = 0; py < height; py++) {
        // 垂直方向の補間率
        uint8_t v_ratio = (py * 255) / (height - 1);

        for (uint16_t px = 0; px < width; px++) {
            // 水平方向の補間率
            uint8_t h_ratio = (px * 255) / (width - 1);

            // 4隅の色を双線形補間
            uint8_t top_color = qd_blend_colors(top_left, top_right, h_ratio);
            uint8_t bottom_color = qd_blend_colors(bottom_left, bottom_right, h_ratio);
            uint8_t final_color = qd_blend_colors(top_color, bottom_color, v_ratio);

            qd_set_pixel_fast(x + px, y + py, final_color);
        }
    }
}

// ============================================================================
// パレットバンク管理機能
// ============================================================================

// パレットバンク（4バンク分）
static uint16_t s_palette_banks[4][16];
static uint8_t s_current_bank = 0;

/**
 * @brief パレットバンクを設定
 *
 * @param bank_index バンクインデックス（0-3）
 */
void qd_set_palette_bank(uint8_t bank_index) {
    if (bank_index >= 4) return;

    // 現在のバンクを保存
    for (uint8_t i = 0; i < 16; i++) {
        s_palette_banks[s_current_bank][i] = s_palette[i];
    }

    // 新しいバンクを復元
    for (uint8_t i = 0; i < 16; i++) {
        s_palette[i] = s_palette_banks[bank_index][i];
    }

    s_current_bank = bank_index;
}

// ============================================================================
// フェーズ2: 円・楕円描画機能
// ============================================================================

/**
 * @brief 円を描画（ブレゼンハムアルゴリズム）
 *
 * @param cx 中心X座標
 * @param cy 中心Y座標
 * @param radius 半径
 * @param color 描画色
 */
void qd_draw_circle(int16_t cx, int16_t cy, uint16_t radius, uint8_t color) {
    if (radius == 0) return;

    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        // 8方向すべてに点を描画
        qd_set_pixel_fast(cx + x, cy + y, color);
        qd_set_pixel_fast(cx + y, cy + x, color);
        qd_set_pixel_fast(cx - y, cy + x, color);
        qd_set_pixel_fast(cx - x, cy + y, color);
        qd_set_pixel_fast(cx - x, cy - y, color);
        qd_set_pixel_fast(cx - y, cy - x, color);
        qd_set_pixel_fast(cx + y, cy - x, color);
        qd_set_pixel_fast(cx + x, cy - y, color);

        y++;
        err += 2 * y + 1;

        if (err > x) {
            err -= 2 * x;
            x--;
        }
    }
}

/**
 * @brief 円を塗りつぶし
 *
 * @param cx 中心X座標
 * @param cy 中心Y座標
 * @param radius 半径
 * @param color 塗りつぶし色
 */
void qd_fill_circle(int16_t cx, int16_t cy, uint16_t radius, uint8_t color) {
    if (radius == 0) return;

    // 各行について水平線を描画
    for (int16_t y = -radius; y <= radius; y++) {
        // 現在の行での円の幅を計算
        int32_t discriminant = (int32_t)radius * radius - (int32_t)y * y;
        if (discriminant < 0) continue;

        int16_t width = (int16_t)(2 * sqrt((double)discriminant));

        // 円の中心からのオフセット
        int16_t x_start = cx - width / 2;
        int16_t x_end = x_start + width;

        // クリッピング適用
        if (x_start < 0) x_start = 0;
        if (x_end > QD_SCREEN_WIDTH) x_end = QD_SCREEN_WIDTH;
        if (x_start >= x_end) continue;

        // 水平線を描画
        for (int16_t x = x_start; x < x_end; x++) {
            qd_set_pixel_fast(x, cy + y, color);
        }
    }
}

/**
 * @brief 楕円を描画（ブレゼンハムアルゴリズム）
 *
 * @param cx 中心X座標
 * @param cy 中心Y座標
 * @param rx X方向半径
 * @param ry Y方向半径
 * @param color 描画色
 */
void qd_draw_ellipse(int16_t cx, int16_t cy, uint16_t rx, uint16_t ry, uint8_t color) {
    if (rx == 0 || ry == 0) return;

    // 楕円描画用の変数
    int32_t a = (int32_t)rx * rx;
    int32_t b = (int32_t)ry * ry;
    int32_t two_a = 2 * a;
    int32_t two_b = 2 * b;

    // 領域1: 上半分
    int32_t x = 0;
    int32_t y = ry;
    int32_t px = 0;
    int32_t py = two_a * y;

    // 領域1の初期決定量
    int32_t discriminant = b - a * ry + (a / 4);

    while (px < py) {
        qd_set_pixel_fast(cx + x, cy + y, color);
        qd_set_pixel_fast(cx - x, cy + y, color);
        qd_set_pixel_fast(cx + x, cy - y, color);
        qd_set_pixel_fast(cx - x, cy - y, color);

        x++;
        px += two_b;
        discriminant += px + b;

        if (discriminant > 0) {
            y--;
            py -= two_a;
            discriminant -= py;
        }
    }

    // 領域2: 下半分
    discriminant = (int32_t)b * (x + 1) * (x + 1) + (int32_t)a * (y - 1) * (y - 1) - a * b;

    while (y >= 0) {
        qd_set_pixel_fast(cx + x, cy + y, color);
        qd_set_pixel_fast(cx - x, cy + y, color);
        qd_set_pixel_fast(cx + x, cy - y, color);
        qd_set_pixel_fast(cx - x, cy - y, color);

        y--;
        py -= two_a;
        discriminant += a - py;

        if (discriminant <= 0) {
            x++;
            px += two_b;
            discriminant += px;
        }
    }
}

/**
 * @brief 楕円を塗りつぶし
 *
 * @param cx 中心X座標
 * @param cy 中心Y座標
 * @param rx X方向半径
 * @param ry Y方向半径
 * @param color 塗りつぶし色
 */
void qd_fill_ellipse(int16_t cx, int16_t cy, uint16_t rx, uint16_t ry, uint8_t color) {
    if (rx == 0 || ry == 0) return;

    // 各行について水平線を描画
    for (int16_t y = -ry; y <= ry; y++) {
        // 現在の行での楕円の幅を計算
        int32_t discriminant = (int32_t)rx * rx - ((int32_t)ry * ry * y * y) / (ry * ry);
        if (discriminant < 0) continue;

        int16_t width = (int16_t)(2 * sqrt((double)discriminant));

        // 楕円の中心からのオフセット
        int16_t x_start = cx - width / 2;
        int16_t x_end = x_start + width;

        // クリッピング適用
        if (x_start < 0) x_start = 0;
        if (x_end > QD_SCREEN_WIDTH) x_end = QD_SCREEN_WIDTH;
        if (x_start >= x_end) continue;

        // 水平線を描画
        for (int16_t x = x_start; x < x_end; x++) {
            qd_set_pixel_fast(x, cy + y, color);
        }
    }
}

/**
 * @brief 現在のパレットバンクを取得
 *
 * @return 現在のバンクインデックス
 */
uint8_t qd_get_palette_bank(void) {
    return s_current_bank;
}

/**
 * @brief パレットバンクを保存
 *
 * @param bank_index 保存先バンクインデックス（0-3）
 */
void qd_save_palette_bank(uint8_t bank_index) {
    if (bank_index >= 4) return;

    for (uint8_t i = 0; i < 16; i++) {
        s_palette_banks[bank_index][i] = s_palette[i];
    }
}

/**
 * @brief パレットバンクを復元
 *
 * @param bank_index 復元元バンクインデックス（0-3）
 */
void qd_restore_palette_bank(uint8_t bank_index) {
    if (bank_index >= 4) return;

    for (uint8_t i = 0; i < 16; i++) {
        s_palette[i] = s_palette_banks[bank_index][i];
    }

    s_current_bank = bank_index;
}