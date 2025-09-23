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

// グローバル変数
static QD_Rect s_clip_rect = {0, 0, QD_SCREEN_WIDTH, QD_SCREEN_HEIGHT};
static volatile uint16_t* s_vram_base = (uint16_t*)0x00c00000;

#ifdef LOCAL_MODE
// LOCAL_MODEではソフトウェアバッファを使用（ワードアライメント保証）
static uint16_t s_local_vram[(QD_SCREEN_WIDTH * QD_SCREEN_HEIGHT) / 2] __attribute__((aligned(4)));
#endif

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