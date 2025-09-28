#pragma once

#include <stdint.h>
#include <stdbool.h>

// Layer構造体の定義
typedef struct Layer {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint8_t* buffer;
    bool is_dirty;
    uint16_t id;
} Layer;

// 関数の宣言
void layer_init(Layer* layer, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t* buffer);
void layer_set_position(Layer* layer, uint16_t x, uint16_t y);
void layer_set_size(Layer* layer, uint16_t width, uint16_t height);
void layer_mark_dirty(Layer* layer);
void layer_clean(Layer* layer);

// レイヤー管理関数
void ss_layer_mgr(void);
void ss_layer_mark_dirty(Layer* layer);
void ss_layer_simple_mark_dirty(Layer* layer);
void ss_layer_draw_simple(void);
Layer* ss_layer_get_simple(uint16_t id);
void ss_layer_set_simple(uint16_t id, Layer* layer);
Layer* ss_layer_get(uint16_t id);
void ss_layer_set(uint16_t id, Layer* layer);

// QuickDraw関連関数（簡易実装）
uint8_t* qd_get_vram_buffer(void);
void qd_get_clip_rect(uint16_t* x, uint16_t* y, uint16_t* width, uint16_t* height);
void qd_set_clip_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void qd_shell_update_taskbar(void);
void qd_monitor_panel_tick(void);
void qd_shell_update_desktop_chrome(void);
void qd_shell_draw_desktop_chrome(void);
void qd_monitor_panel_init(void);
void qd_shell_draw_taskbar(void);