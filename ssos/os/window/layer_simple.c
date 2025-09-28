#include "layer.h"
#include "../main/ssoswindows.h"
#include <string.h>

// 静的なレイヤーインスタンス
static Layer layer_1;
static Layer layer_2;
static Layer layer_3;

// レイヤーの初期化
void layer_init(Layer* layer, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t* buffer) {
    layer->x = x;
    layer->y = y;
    layer->width = width;
    layer->height = height;
    layer->buffer = buffer;
    layer->is_dirty = false;
    layer->id = 0;
}

// レイヤーの位置を設定
void layer_set_position(Layer* layer, uint16_t x, uint16_t y) {
    layer->x = x;
    layer->y = y;
    layer_mark_dirty(layer);
}

// レイヤーのサイズを設定
void layer_set_size(Layer* layer, uint16_t width, uint16_t height) {
    layer->width = width;
    layer->height = height;
    layer_mark_dirty(layer);
}

// レイヤーをダーティとしてマーク
void layer_mark_dirty(Layer* layer) {
    layer->is_dirty = true;
}

// レイヤーをクリーンとしてマーク
void layer_clean(Layer* layer) {
    layer->is_dirty = false;
}

// レイヤー互換性レイヤーの実装は main/ssoswindows.c で定義されているため、ここでは定義しない

// レイヤー管理関数の簡易実装
void ss_layer_mgr(void) {
    // レイヤー管理のメイン処理（簡易実装）
}

void ss_layer_mark_dirty(Layer* layer) {
    if (layer) {
        layer_mark_dirty(layer);
    }
}

void ss_layer_simple_mark_dirty(Layer* layer) {
    if (layer) {
        layer_mark_dirty(layer);
    }
}

void ss_layer_draw_simple(void) {
    // シンプルレイヤーの描画処理（簡易実装）
}

// グローバルレイヤーのインスタンス
static Layer simple_layers[3];

Layer* ss_layer_get_simple(uint16_t id) {
    if (id < 3) {
        return &simple_layers[id];
    }
    return NULL;
}

void ss_layer_set_simple(uint16_t id, Layer* layer) {
    if (id < 3 && layer) {
        simple_layers[id] = *layer;
    }
}

Layer* ss_layer_get(uint16_t id) {
    return ss_layer_get_simple(id);
}

void ss_layer_set(uint16_t id, Layer* layer) {
    ss_layer_set_simple(id, layer);
}

// QuickDraw関連関数の簡易実装
uint8_t* qd_get_vram_buffer(void) {
    // VRAMバッファの取得（簡易実装）
    static uint8_t vram_buffer[160*100]; // さらに小さなバッファに変更
    return vram_buffer;
}

void qd_get_clip_rect(uint16_t* x, uint16_t* y, uint16_t* width, uint16_t* height) {
    // クリップ矩形の取得（簡易実装）
    if (x) *x = 0;
    if (y) *y = 0;
    if (width) *width = 640;
    if (height) *height = 400;
}

void qd_set_clip_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    // クリップ矩形の設定（簡易実装）
    // 実際の実装ではクリップ矩形を保存する
}

void qd_shell_update_taskbar(void) {
    // タスクバーの更新（簡易実装）
}

void qd_monitor_panel_tick(void) {
    // モニタパネルのティック処理（簡易実装）
}

void qd_shell_update_desktop_chrome(void) {
    // デスクトップクロムの更新（簡易実装）
}

void qd_shell_draw_desktop_chrome(void) {
    // デスクトップクロムの描画（簡易実装）
}

void qd_monitor_panel_init(void) {
    // モニタパネルの初期化（簡易実装）
}

void qd_shell_draw_taskbar(void) {
    // タスクバーの描画（簡易実装）
}