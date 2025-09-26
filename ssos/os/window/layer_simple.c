/**
 * SX-Window互換のシンプルなLayerシステム
 * X68000 10MHzで実際に高速動作する実装
 *
 * 問題点の分析:
 * 1. 現在のLayerシステムはメモリマップを8x8ピクセル単位でチェックしすぎ
 * 2. 複雑な最適化（キャッシュ、バッファプール、段階的初期化）がオーバーヘッドを生む
 * 3. DMA転送の初期化コストが高い
 * 4. メモリマップの構築・更新コストが高い
 *
 * SX-Windowの高速動作の秘密:
 * 1. メモリマップなし - 直接座標計算で重なりチェック
 * 2. シンプルなアルゴリズム - 一切のキャッシュなし
 * 3. 連続領域のまとめ転送 - DMAの効率的利用
 * 4. 最小限のデータ構造 - 余計なメタデータなし
 */

#include "layer.h"
#include "ssoswindows.h"
#include "dma.h"
#include "kernel.h"
#include "memory.h"
#include "vram.h"
#include <stddef.h>
#include <stdio.h>

// 超シンプルなメモリマップ（8x8ブロックに戻す）
#define SIMPLE_MAP_WIDTH (768 / 8)  // 96
#define SIMPLE_MAP_HEIGHT (512 / 8) // 64
static uint8_t s_simple_map[SIMPLE_MAP_HEIGHT][SIMPLE_MAP_WIDTH];

// 現在のLayerシステムと連携するためのグローバル変数
extern LayerMgr* ss_layer_mgr;
extern const int WIDTH;
extern const int HEIGHT;
extern const int VRAMWIDTH;

// シンプルシステム用の内部変数
static int s_layer_count = 0;

// SX-Windowスタイルの高速初期化
void ss_layer_init_simple(void) {
    // メモリマップをゼロクリア（8x8ブロック単位）
    for (int y = 0; y < SIMPLE_MAP_HEIGHT; y++) {
        for (int x = 0; x < SIMPLE_MAP_WIDTH; x++) {
            s_simple_map[y][x] = 0;
        }
    }
    s_layer_count = 0;

    // LayerMgrの初期化も必要（zLayers配列も初期化）
    if (!ss_layer_mgr) {
        ss_layer_mgr = (LayerMgr*)ss_mem_alloc4k(sizeof(LayerMgr));
        ss_layer_mgr->topLayerIdx = 0;
        for (int i = 0; i < MAX_LAYERS; i++) {
            ss_layer_mgr->layers[i].attr = 0;
            ss_layer_mgr->zLayers[i] = NULL;
        }
        ss_layer_mgr->initialized = true;
    } else {
        // 既存のLayerMgrの場合もzLayersをリセット
        for (int i = 0; i < MAX_LAYERS; i++) {
            ss_layer_mgr->zLayers[i] = NULL;
        }
        ss_layer_mgr->topLayerIdx = 0;
    }
}

// SX-Windowスタイルの高速レイヤー作成
Layer* ss_layer_get_simple(void) {
    if (!ss_layer_mgr) return NULL;

    // LayerMgrのlayers配列から空きレイヤーを探す
    for (int i = 0; i < MAX_LAYERS; i++) {
        if ((ss_layer_mgr->layers[i].attr & LAYER_ATTR_USED) == 0) {
            Layer* l = &ss_layer_mgr->layers[i];
            l->attr = LAYER_ATTR_USED | LAYER_ATTR_VISIBLE;
            l->z = i;  // z順は配列インデックス
            l->vram = NULL;
            l->needs_redraw = 1;

            // zLayers配列に登録
            ss_layer_mgr->zLayers[ss_layer_mgr->topLayerIdx] = l;
            ss_layer_mgr->topLayerIdx++;

            return l;
        }
    }
    return NULL;
}

// 超シンプルなレイヤー描画（メモリマップチェック最小化）
void ss_layer_draw_simple_layer(Layer* l) {
    if (!l || !l->vram) return;
    if ((l->attr & LAYER_ATTR_VISIBLE) == 0) return;

    ss_layer_draw_rect_layer_simple(l);
}

// SX-Windowスタイルの高速描画
// NOTE: この簡易バックエンドは重なり領域の再描画抑制をまだ実装していないため、
//       上位レイヤー更新時にも下位レイヤーが描画される制限がある。
void ss_layer_draw_simple(void) {
    if (!ss_layer_mgr) return;

    // 下位レイヤーから順に描画し、最上位レイヤーを最後に適用する
    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* l = ss_layer_mgr->zLayers[i];
        if (l && (l->attr & LAYER_ATTR_VISIBLE) && l->vram) {
            ss_layer_draw_simple_layer(l);
        }
    }
}

// レイヤー設定（メモリマップ更新を最小化）
void ss_layer_set_simple(Layer* layer, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    layer->x = x;
    layer->y = y;
    layer->w = w;
    layer->h = h;

    // メモリマップ更新（変更された領域のみ、8x8ブロック単位）
    uint16_t start_map_x = x >> 3;
    uint16_t start_map_y = y >> 3;
    uint16_t end_map_x = (x + w + 7) >> 3;
    uint16_t end_map_y = (y + h + 7) >> 3;

    // 範囲チェック
    if (start_map_x >= SIMPLE_MAP_WIDTH) start_map_x = SIMPLE_MAP_WIDTH - 1;
    if (start_map_y >= SIMPLE_MAP_HEIGHT) start_map_y = SIMPLE_MAP_HEIGHT - 1;
    if (end_map_x > SIMPLE_MAP_WIDTH) end_map_x = SIMPLE_MAP_WIDTH;
    if (end_map_y > SIMPLE_MAP_HEIGHT) end_map_y = SIMPLE_MAP_HEIGHT;

    // メモリマップ更新（z順に基づく）
    for (uint16_t my = start_map_y; my < end_map_y; my++) {
        for (uint16_t mx = start_map_x; mx < end_map_x; mx++) {
            s_simple_map[my][mx] = layer->z;
        }
    }
}

// 高速VRAM転送（SX-Windowスタイル）
void ss_layer_blit_fast(Layer* l, uint16_t dx, uint16_t dy, uint16_t dw, uint16_t dh) {
    if (!l->vram) return;

    // 範囲チェック
    if (dx >= WIDTH || dy >= HEIGHT) return;
    if (dx + dw > WIDTH) dw = WIDTH - dx;
    if (dy + dh > HEIGHT) dh = HEIGHT - dy;

    // 連続領域をまとめて転送
    uint8_t* src = l->vram;
    uint8_t* dst = (uint8_t*)&vram_start[dy * VRAMWIDTH + dx];

    // 単純なmemcpyで高速転送
    for (int y = 0; y < dh; y++) {
        uint8_t* src_line = src + y * l->w;
        uint8_t* dst_line = dst + y * VRAMWIDTH;
        for (int x = 0; x < dw; x++) {
            dst_line[x] = src_line[x];
        }
    }
}

// 超高速DMA転送（初期化コスト削減）
void ss_layer_draw_rect_layer_simple(Layer* l) {
    if (!l->vram) return;

    // 画面範囲外チェック
    if (l->x >= WIDTH || l->y >= HEIGHT) return;
    if (l->x + l->w <= 0 || l->y + l->h <= 0) return;

    // クリッピング
    int16_t start_x = l->x < 0 ? 0 : l->x;
    int16_t start_y = l->y < 0 ? 0 : l->y;
    int16_t end_x = (l->x + l->w > WIDTH) ? WIDTH : l->x + l->w;
    int16_t end_y = (l->y + l->h > HEIGHT) ? HEIGHT : l->y + l->h;

    // DMAで高速転送（連続領域をまとめて）
    uint8_t* src = l->vram + (start_y - l->y) * l->w + (start_x - l->x);

    const int16_t width = end_x - start_x;
    const int16_t height = end_y - start_y;

    // X68000 16色モード: 1ピクセルあたり2バイト（偶数バイトがプレーンデータ、奇数バイトにインデックス）
    const int32_t vram_stride_bytes = VRAMWIDTH * 2;
    uint8_t* dst_base = ((uint8_t*)vram_start) + ((start_y * VRAMWIDTH) + start_x) * 2 + 1;

    for (int y = 0; y < height; y++) {
        uint8_t* src_line = src + y * l->w;
        uint8_t* dst_line = dst_base + y * vram_stride_bytes;

        for (int x = 0; x < width; x++) {
            dst_line[x * 2] = src_line[x];
        }
    }
}

// パフォーマンス測定用
void ss_layer_benchmark_simple(void) {
    uint32_t start_time = ss_timerd_counter;

    // 100フレーム分の描画
    for (int frame = 0; frame < 100; frame++) {
        ss_layer_draw_simple();
    }

    uint32_t end_time = ss_timerd_counter;
    uint32_t total_time = end_time - start_time;

    // printf("Simple Layer System Performance:\n");
    // printf("100 frames: %lu cycles\n", total_time);
    // printf("Average: %.2f cycles/frame\n", (float)total_time / 100.0f);
    // printf("FPS equivalent: %.1f\n", 1000000.0f / (total_time / 100.0f));
}

// メモリ使用量レポート
void ss_layer_report_memory_simple(void) {
    uint32_t map_memory = sizeof(s_simple_map);
    uint32_t layer_memory = (ss_layer_mgr ? ss_layer_mgr->topLayerIdx * sizeof(Layer) : 0);

    // printf("Simple Layer Memory Usage:\n");
    // printf("Map memory: %lu bytes\n", map_memory);
    // printf("Layer memory: %lu bytes\n", layer_memory);
    // printf("Total: %lu bytes\n", map_memory + layer_memory);
}
