#include "layer.h"
#include "kernel.h"
#include "memory.h"
#include "vram.h"
#include <stddef.h>

LayerMgr* ss_layer_mgr;

void ss_layer_init() {
    ss_layer_mgr = (LayerMgr*)ss_mem_alloc4k(sizeof(LayerMgr));
    ss_layer_mgr->topLayerIdx = 0;
    for (int i = 0; i < MAX_LAYERS; i++) {
        ss_layer_mgr->layers[i].attr = 0;
    }
}

Layer* ss_layer_get() {
    for (int i = 0; i < MAX_LAYERS; i++) {
        if ((ss_layer_mgr->layers[i].attr & LAYER_ATTR_USED) == 0) {
            ss_layer_mgr->layers[i].attr =
                (LAYER_ATTR_USED | LAYER_ATTR_VISIBLE);
            ss_layer_mgr->layers[i].z = 0;
            ss_layer_mgr->zLayers[ss_layer_mgr->topLayerIdx] =
                &ss_layer_mgr->layers[i];
            ss_layer_mgr->topLayerIdx++;
            return &ss_layer_mgr->layers[i];
        }
    }
    return NULL;
}

void ss_layer_set(Layer* layer, uint16_t* vram, uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h) {
    layer->vram = vram;
    layer->x = x;
    layer->y = y;
    layer->w = w;
    layer->h = h;
}

void ss_layer_set_z(Layer* layer, uint16_t z) {
    uint16_t prev = layer->z;

    // if (z < 0) {
    //     z = 0;
    // }
    // if (z > ss_layer_mgr->topLayerIdx + 1) {
    //     z = ss_layer_mgr->topLayerIdx + 1;
    // }
    // layer->z = z;

    // // reorder zLayers
    // if (prev > z) {
    //     // lower than the previous z
    //     for (int i = prev; i > z; i--) {
    //         ss_layer_mgr->zLayers[i] = ss_layer_mgr->zLayers[i - 1];
    //         ss_layer_mgr->zLayers[i]->z = i;
    //     }
    //     ss_layer_mgr->zLayers[z] = layer;
    // } else if (prev < z) {
    //     // higher than the previous z
    //     for (int i = prev; i < z; i++) {
    //         ss_layer_mgr->zLayers[i] = ss_layer_mgr->zLayers[i + 1];
    //         ss_layer_mgr->zLayers[i]->z = i;
    //     }
    //     ss_layer_mgr->zLayers[z] = layer;
    // }
    // ss_layer_draw();
}

void ss_layer_draw() { ss_layer_draw_rect(0, 0, WIDTH, HEIGHT); }

void ss_layer_draw_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* layer = ss_layer_mgr->zLayers[i];

        uint16_t bx0 = x0 - layer->x;
        uint16_t by0 = y0 - layer->y;
        uint16_t bx1 = x1 - layer->x;
        uint16_t by1 = y1 - layer->y;
        if (bx0 < 0)
            bx0 = 0;
        if (by0 < 0)
            by0 = 0;
        if (bx1 > layer->w)
            bx1 = layer->w;
        if (by1 > layer->h)
            by1 = layer->h;
        for (uint16_t by = by0; by < by1; by++) {
            uint16_t vy = layer->y + by;
            if (vy < 0 || vy >= HEIGHT)
                continue;
            for (uint16_t bx = bx0; bx < bx1; bx++) {
                uint16_t vx = layer->x + bx;
                if (vx < 0 || vx >= WIDTH)
                    continue;
                vram_start[vy * VRAMWIDTH + vx] =
                    layer->vram[by * layer->w + bx];
            }
        }
        // if(layer->attr & LAYER_ATTR_VISIBLE){
        //     for(int y=0;y<layer->h;y++){
        //         int blocks = layer->w / 4;
        //         int rest = layer->w % 4;
        //         // draw per 16byte block for faster drawing
        //         for(int b=0;b<blocks;b++){
        //             uint16_t* src = &layer->vram[y * layer->w + b * 4];
        //             uint16_t* dst = &vram_start[(layer->y + y) * VRAMWIDTH +
        //             layer->x + b * 4]; dst[0] = src[0]; dst[1] = src[1];
        //             dst[2] = src[2];
        //             dst[3] = src[3];
        //         }
        //         // draw the rest
        //         for(int r=0;r<rest;r++){
        //             vram_start[(layer->y + y) * VRAMWIDTH + layer->x + blocks
        //             * 4 + r] = layer->vram[y * layer->w + blocks * 4 + r];
        //         }
        //     }
        // }
    }
}

void ss_layer_move(Layer* layer, uint16_t x, uint16_t y) {
    uint16_t x0 = layer->x;
    uint16_t y0 = layer->y;
    layer->x = x;
    layer->y = y;
    ss_layer_draw_rect(x0, y0, x0 + layer->w, y0 + layer->h);
    ss_layer_draw_rect(x, y, x + layer->w, y + layer->h);
}