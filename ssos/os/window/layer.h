#pragma once
#include <stdbool.h>
#include <stdint.h>

#define MAX_LAYERS 256

enum LayerAttr {
    LAYER_ATTR_USED = 0x01,
    LAYER_ATTR_VISIBLE = 0x02,
};

typedef struct {
    uint16_t x, y, z;
    uint16_t w, h;
    uint16_t attr;
    uint16_t* vram;
} Layer;

typedef struct {
    int16_t topLayerIdx;
    Layer* zLayers[MAX_LAYERS]; // z-order sorted layer pointers
    Layer layers[MAX_LAYERS];   // layer data ( sizeof(Layer) * MAX_LAYERS )
} LayerMgr;

extern LayerMgr* ss_layer_mgr;

void ss_layer_init();
Layer* ss_layer_get();
void ss_layer_set(Layer* layer, uint16_t* vram, uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h);
void ss_layer_set_z(Layer* layer, uint16_t z);
void ss_all_layer_draw();
void ss_all_layer_draw_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ss_layer_move(Layer* layer, uint16_t x, uint16_t y);
void ss_layer_invalidate(Layer* layer);