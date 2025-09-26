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
#include <string.h>

// 超シンプルなメモリマップ（8x8ブロックに戻す）
#define SIMPLE_MAP_WIDTH (768 / 8)   // 96
#define SIMPLE_MAP_HEIGHT (512 / 8)  // 64
#define SIMPLE_MAP_EMPTY 0xFF
static uint8_t s_simple_map[SIMPLE_MAP_HEIGHT][SIMPLE_MAP_WIDTH];

// 現在のLayerシステムと連携するためのグローバル変数
extern LayerMgr* ss_layer_mgr;
extern const int WIDTH;
extern const int HEIGHT;
extern const int VRAMWIDTH;

// シンプルシステム用の内部変数
static int s_layer_count = 0;

static void ss_layer_simple_clear_map(void);
static void ss_layer_simple_rebuild_occlusion(void);
static bool ss_layer_simple_any_dirty(void);
void ss_layer_simple_mark_dirty(Layer* layer, bool include_lower_layers);

// SX-Windowスタイルの高速初期化
void ss_layer_init_simple(void) {
    // メモリマップをゼロクリア（8x8ブロック単位）
    ss_layer_simple_clear_map();
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
            l->z = (uint16_t)ss_layer_mgr->topLayerIdx;
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

static void ss_layer_simple_clear_map(void) {
    memset(s_simple_map, SIMPLE_MAP_EMPTY, sizeof(s_simple_map));
}

static bool ss_layer_simple_any_dirty(void) {
    if (!ss_layer_mgr) return false;
    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* l = ss_layer_mgr->zLayers[i];
        if (!l) continue;
        if ((l->attr & LAYER_ATTR_VISIBLE) == 0) continue;
        if (l->needs_redraw) return true;
    }
    return false;
}

static void ss_layer_simple_rebuild_occlusion(void) {
#ifdef SS_LAYER_SIMPLE_DISABLE_OCCLUSION
    // Nothing to do when occlusion is disabled
    (void)s_simple_map;
#else
    ss_layer_simple_clear_map();
    if (!ss_layer_mgr) return;

    for (int i = ss_layer_mgr->topLayerIdx - 1; i >= 0; i--) {
        Layer* l = ss_layer_mgr->zLayers[i];
        if (!l) continue;
        if ((l->attr & LAYER_ATTR_VISIBLE) == 0) continue;
        if (!l->vram) continue;

        int16_t x0 = l->x;
        int16_t y0 = l->y;
        int16_t x1 = l->x + l->w;
        int16_t y1 = l->y + l->h;

        if (x1 <= 0 || y1 <= 0 || x0 >= WIDTH || y0 >= HEIGHT) {
            continue;
        }

        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > WIDTH) x1 = WIDTH;
        if (y1 > HEIGHT) y1 = HEIGHT;

        uint16_t start_map_x = (uint16_t)(x0 >> 3);
        uint16_t start_map_y = (uint16_t)(y0 >> 3);
        uint16_t end_map_x = (uint16_t)((x1 + 7) >> 3);
        uint16_t end_map_y = (uint16_t)((y1 + 7) >> 3);

        if (end_map_x > SIMPLE_MAP_WIDTH) end_map_x = SIMPLE_MAP_WIDTH;
        if (end_map_y > SIMPLE_MAP_HEIGHT) end_map_y = SIMPLE_MAP_HEIGHT;

        for (uint16_t my = start_map_y; my < end_map_y; my++) {
            for (uint16_t mx = start_map_x; mx < end_map_x; mx++) {
                if (s_simple_map[my][mx] == SIMPLE_MAP_EMPTY) {
                    s_simple_map[my][mx] = (uint8_t)l->z;
                }
            }
        }
    }
#endif
}

// 超シンプルなレイヤー描画（メモリマップチェック最小化）
void ss_layer_draw_simple_layer(Layer* l) {
    if (!l || !l->vram) return;
    if ((l->attr & LAYER_ATTR_VISIBLE) == 0) return;

    ss_layer_draw_rect_layer_simple(l);
}

// SX-Windowスタイルの高速描画
// NOTE: 8x8ブロック単位の簡易オクルージョンマップで上位レイヤーに完全に覆われた
//       領域の再描画を抑制する（部分的に重なる場合は最小ブロック単位で描画）。
void ss_layer_draw_simple(void) {
    if (!ss_layer_mgr) return;
    if (!ss_layer_simple_any_dirty()) {
        return;
    }

#ifndef SS_LAYER_SIMPLE_DISABLE_OCCLUSION
    ss_layer_simple_rebuild_occlusion();
#endif

    // 下位レイヤーから順に描画し、最上位レイヤーを最後に適用する
    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* l = ss_layer_mgr->zLayers[i];
        if (!l) continue;
        if ((l->attr & LAYER_ATTR_VISIBLE) == 0) continue;
        if (!l->vram) continue;
        if (!l->needs_redraw) continue;

        ss_layer_draw_simple_layer(l);
    }
}

// レイヤー設定（メモリマップ更新を最小化）
void ss_layer_set_simple(Layer* layer, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t aligned_x = (uint16_t)(x & ~0x7);
    uint16_t aligned_y = (uint16_t)(y & ~0x7);

    uint32_t requested_end_x = x + w;
    uint32_t requested_end_y = y + h;
    if (requested_end_x > (uint32_t)WIDTH) requested_end_x = WIDTH;
    if (requested_end_y > (uint32_t)HEIGHT) requested_end_y = HEIGHT;

    uint16_t aligned_end_x = (uint16_t)(((requested_end_x + 7u) & ~7u));
    uint16_t aligned_end_y = (uint16_t)(((requested_end_y + 7u) & ~7u));

    if (aligned_end_x > (uint16_t)WIDTH) aligned_end_x = (uint16_t)WIDTH;
    if (aligned_end_y > (uint16_t)HEIGHT) aligned_end_y = (uint16_t)HEIGHT;

    uint16_t aligned_w = aligned_end_x > aligned_x ? (aligned_end_x - aligned_x) : 8;
    uint16_t aligned_h = aligned_end_y > aligned_y ? (aligned_end_y - aligned_y) : 8;

    layer->x = aligned_x;
    layer->y = aligned_y;
    layer->w = aligned_w;
    layer->h = aligned_h;

    ss_layer_simple_mark_dirty(layer, true);
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

        int16_t world_y = start_y + y;
        uint16_t map_y = (uint16_t)(world_y >> 3);

        for (int16_t px = 0; px < width; ) {
            int16_t world_x = start_x + px;
            uint16_t map_x = (uint16_t)(world_x >> 3);

            int16_t block_end = ((world_x & ~7) + 8);
            if (block_end > start_x + width) {
                block_end = start_x + width;
            }

            bool occluded = false;
#ifndef SS_LAYER_SIMPLE_DISABLE_OCCLUSION
            if (map_y < SIMPLE_MAP_HEIGHT && map_x < SIMPLE_MAP_WIDTH) {
                uint8_t top_z = s_simple_map[map_y][map_x];
                if (top_z != SIMPLE_MAP_EMPTY && top_z > l->z) {
                    occluded = true;
                }
            }
#endif

            int16_t segment_len = block_end - (start_x + px);

            if (!occluded) {
                uint8_t* src_seg = src_line + px;
                uint8_t* dst_seg = dst_line + px * 2;
                for (int sx = 0; sx < segment_len; sx++) {
                    dst_seg[sx * 2] = src_seg[sx];
                }
            }

            px += segment_len;
        }
    }

    l->needs_redraw = 0;
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

void ss_layer_simple_mark_dirty(Layer* layer, bool include_lower_layers) {
    if (!layer) return;
    layer->needs_redraw = 1;

    if (!include_lower_layers) {
        return;
    }

    if (!ss_layer_mgr) return;

    bool mark_remaining = true;
    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* current = ss_layer_mgr->zLayers[i];
        if (!current) continue;
        if (current == layer) {
            mark_remaining = false;
            continue;
        }
        if (mark_remaining) {
            current->needs_redraw = 1;
        }
    }
}
