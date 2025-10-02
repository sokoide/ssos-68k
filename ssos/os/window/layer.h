#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "ss_config.h"

enum LayerAttr {
    LAYER_ATTR_USED = 0x01,
    LAYER_ATTR_VISIBLE = 0x02,
};

typedef struct {
    uint16_t x, y, z;
    uint16_t w, h;
    uint16_t attr;
    uint8_t* vram;
    // Dirty rectangle tracking for performance
    uint16_t dirty_x, dirty_y, dirty_w, dirty_h;
    uint8_t needs_redraw;
} Layer;

typedef struct {
    int16_t topLayerIdx;
    Layer* zLayers[MAX_LAYERS]; // z-order sorted layer pointers
    Layer layers[MAX_LAYERS];   // layer data ( sizeof(Layer) * MAX_LAYERS )
    uint8_t* map;
} LayerMgr;

extern LayerMgr* ss_layer_mgr;

void ss_layer_init();
Layer* ss_layer_get();
void ss_layer_set(Layer* layer, uint8_t* vram, uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h);
void ss_all_layer_draw();

void ss_layer_draw_rect_layer_dma(Layer* l, uint8_t* src, uint8_t* dst,
                                  uint16_t block_count);

void ss_layer_invalidate(Layer* layer);
void ss_layer_mark_dirty(Layer* layer, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void ss_layer_mark_clean(Layer* layer);
void ss_layer_draw_dirty_only();
void ss_layer_draw_rect_layer_bounds(Layer* l, uint16_t dx0, uint16_t dy0, uint16_t dx1, uint16_t dy1);

// Window management functions
Layer* ss_layer_find_at_position(uint16_t x, uint16_t y);
void ss_layer_bring_to_front(Layer* layer);
void ss_layer_set_z_order(Layer* layer, uint16_t new_z);
void ss_layer_rebuild_z_map(void);
