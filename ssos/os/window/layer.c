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
            ss_layer_mgr->layers[i].z = ss_layer_mgr->topLayerIdx;
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
    // uint16_t prev = layer->z;

    // TODO:
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

void ss_all_layer_draw() { ss_all_layer_draw_rect(0, 0, WIDTH, HEIGHT); }

// Draw the rectangle area (x0, y0) - (x1, y1)
// in vram coordinates
void ss_all_layer_draw_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    ss_z_layer_draw_rect(0, x0, y0, x1, y1);
}

// Draw the rectangle area (x0, y0) - (x1, y1)
// in vram coordinates whose z is z or higher
void ss_z_layer_draw_rect(uint16_t z, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    for (int i = z; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* layer = ss_layer_mgr->zLayers[i];
        if (0 == (layer->attr & LAYER_ATTR_VISIBLE))
            continue;

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

#if 0
        // (dx0, dy0) - (dx1, dy1) is the intersection of the layer and the
        // rectangle (x0, y0) - (x1, y1).
        // layer->x+dx0, layer->y+dy0 - layer->x+dx1, layer->y+dy1 will be
        // redrawn.
        uint16_t dx0 = x0 - layer->x;
        uint16_t dy0 = y0 - layer->y;
        uint16_t dx1 = x1 - layer->x;
        uint16_t dy1 = y1 - layer->y;
        if (dx0 < 0)
            dx0 = 0;
        if (dy0 < 0)
            dy0 = 0;
        if (dx1 > layer->w)
            dx1 = layer->w;
        if (dy1 > layer->h)
            dy1 = layer->h;

        // (vx, vy) is the target vram coordinates to draw.
        for (uint16_t dy = dy0; dy < dy1; dy++) {
            uint16_t vy = layer->y + dy;
            if (vy < 0 || vy >= HEIGHT)
                continue;

            // draw per 16byte block for faster drawing
            // uint16_t blocks = (dx1 - dx0) / 4;
            // uint16_t rest = (dx1 - dx0) % 4;
            // for (uint16_t b = 0; b < blocks; b++) {
            //     uint16_t vx = layer->x + dx0 + b * 4;

            //     uint16_t* src = &layer->vram[dy * layer->w + dx0 + b * 4];
            //     volatile uint16_t* dst =
            //         &vram_start[vy * VRAMWIDTH + vx];
            //     dst[0] = src[0];
            //     dst[1] = src[1];
            //     dst[2] = src[2];
            //     dst[3] = src[3];
            // }
            // draw the rest
            // for (uint16_t r = 0; r < rest; r++) {
            //     uint16_t vx = layer->x + blocks * 4 + dx0 + r;
            //     vram_start[vy * VRAMWIDTH + vx] =
            //         layer->vram[dy * layer->w + blocks * 4 + dx0 + r];
            // }
        }
#endif
    }
}

void ss_layer_move(Layer* layer, uint16_t x, uint16_t y) {
    uint16_t prevx = layer->x;
    uint16_t prevy = layer->y;
    layer->x = x;
    layer->y = y;
    ss_all_layer_draw_rect(prevx, prevy, prevx + layer->w, prevy + layer->h);
    ss_all_layer_draw_rect(x, y, x + layer->w, y + layer->h);
}

void ss_layer_invalidate(Layer* layer) {
    ss_z_layer_draw_rect(layer->z, layer->x, layer->y, layer->x + layer->w,
        layer->y + layer->h);
}