/**
 * SX-Window互換のシンプルなLayerシステム - ヘッダーファイル
 * X68000 10MHzで実際に高速動作する実装
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

// SX-Windowスタイルの定数
#define MAX_LAYERS 16  // シンプルな制限

// SX-Windowスタイルの超シンプルなLayer構造体
typedef struct {
    uint16_t x, y;              // 位置
    uint16_t w, h;              // サイズ
    uint16_t attr;              // 属性
    uint8_t z;                  // Z-order (0-255)
    uint8_t* vram;              // VRAMバッファ
    uint8_t needs_redraw;       // 再描画フラグ
} SimpleLayer;

// 外部参照のグローバル変数
extern SimpleLayer s_direct_layers[MAX_LAYERS];
extern int s_layer_count;

// SX-Windowスタイルの高速関数
void ss_layer_init_simple(void);
SimpleLayer* ss_layer_get_simple(void);
void ss_layer_draw_simple(void);
void ss_layer_draw_simple_layer(SimpleLayer* l);
void ss_layer_set_simple(SimpleLayer* layer, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ss_layer_blit_fast(SimpleLayer* l, uint16_t dx, uint16_t dy, uint16_t dw, uint16_t dh);
void ss_layer_draw_rect_layer_simple(SimpleLayer* l);

// パフォーマンス測定用
void ss_layer_benchmark_simple(void);
void ss_layer_report_memory_simple(void);