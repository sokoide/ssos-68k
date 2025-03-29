#include "layer.h"
#include "dma.h"
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
    ss_layer_mgr->map = (uint8_t*)ss_mem_alloc4k(WIDTH * HEIGHT);
    // reset to 0 every 4 bytes
    uint32_t* p = (uint32_t*)ss_layer_mgr->map;
    for (int i = 0; i < WIDTH * HEIGHT / 4; i++) {
        *p++ = 0;
    }
}

Layer* ss_layer_get() {
    Layer* l = NULL;
    for (int i = 0; i < MAX_LAYERS; i++) {
        if ((ss_layer_mgr->layers[i].attr & LAYER_ATTR_USED) == 0) {
            l = &ss_layer_mgr->layers[i];
            l->attr = (LAYER_ATTR_USED | LAYER_ATTR_VISIBLE);
            l->z = ss_layer_mgr->topLayerIdx;
            ss_layer_mgr->zLayers[ss_layer_mgr->topLayerIdx] = l;
            ss_layer_mgr->topLayerIdx++;

            return l;
        }
    }
    return l;
}

void ss_layer_set(Layer* layer, uint8_t* vram, uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h) {
    layer->vram = vram;
    layer->x = x;
    layer->y = y;
    layer->w = w;
    layer->h = h;

    uint8_t lid = layer - ss_layer_mgr->layers;
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            ss_layer_mgr->map[(y + dy) * WIDTH + (x + dx)] = lid;
        }
    }
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
void ss_all_layer_draw_rect(uint16_t x0, uint16_t y0, uint16_t x1,
                            uint16_t y1) {
    for (int i = 0; i < ss_layer_mgr->topLayerIdx; i++) {
        Layer* layer = ss_layer_mgr->zLayers[i];
        ss_layer_draw_rect_layer(layer, x0, y0, x1, y1);
    }
}

// Draw the rectangle area (x0, y0) - (x1, y1)
// in vram coordinates for the layer id
void ss_layer_draw_rect_layer(Layer* l, uint16_t x0, uint16_t y0, uint16_t x1,
                              uint16_t y1) {
    if (0 == (l->attr & LAYER_ATTR_VISIBLE))
        return;
    uint8_t lid = l - ss_layer_mgr->layers;

    // (dx0, dy0) - (dx1, dy1) is the intersection of the layer and the
    // rectangle (x0, y0) - (x1, y1).
    // l->x+dx0, l->y+dy0 - l->x+dx1, l->y+dy1 will be
    // redrawn.
    int16_t dx0 = x0 - l->x;
    int16_t dy0 = y0 - l->y;
    int16_t dx1 = x1 - l->x;
    int16_t dy1 = y1 - l->y;
    if (dx0 < 0)
        dx0 = 0;
    if (dy0 < 0)
        dy0 = 0;
    if (dx1 > l->w)
        dx1 = l->w;
    if (dy1 > l->h)
        dy1 = l->h;
    for (int16_t dy = dy0; dy < dy1; dy++) {
        int16_t vy = l->y + dy;
        if (vy < 0 || vy >= HEIGHT)
            continue;
        // DMA transfer
        // draw dx1-dx0 dots
        dma_clear();
        // only 1 line (block)
        int16_t vx = l->x + dx0;
        // src offscreen vram addr, block count
        // target vram addr must be an add addr
        dma_init(((uint8_t*)&vram_start[vy * VRAMWIDTH + vx]) + 1, 1);
        // dst vram addr
        xfr_inf[0].addr = &l->vram[dy * l->w + dx0];
        xfr_inf[0].count = dx1 - dx0;
        // transfer
        dma_start();
        dma_wait_completion();
        dma_clear();
#if 0
        // draw per K dots (2*K bytes) for faster drawing
        const int K = 8;
        int16_t blocks = (dx1 - dx0) / K;
        int16_t rest = (dx1 - dx0) % K;

        for (int16_t b = 0; b < blocks; b++) {
            int16_t vx = l->x + dx0 + b * K;
            int8_t* src = &l->vram[dy * l->w + dx0 + b * K];
            int16_t* dst = &vram_start[vy * VRAMWIDTH + vx];
            if (ss_layer_mgr->map[vy * WIDTH + vx] == l->z) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = src[3];
                dst[4] = src[4];
                dst[5] = src[5];
                dst[6] = src[6];
                dst[7] = src[7];
            }
        }
        // draw the rest
        int16_t vx = l->x + dx0 + blocks * K;
        for (int16_t r = 0; r < rest; r++) {
            if (ss_layer_mgr->map[vy * WIDTH + vx + r] == l->z) {
                vram_start[vy * VRAMWIDTH + vx + r] =
                    l->vram[dy * l->w + blocks * K + dx0 + r];
            }
        }
#endif

#if 0
        // original code (slow)
        for (uint16_t dx = dx0; dx < dx1; dx++) {
            uint16_t vx = layer->x + dx;
            if (vx < 0 || vx >= WIDTH)
                continue;
            vram_start[vy * VRAMWIDTH + vx] =
                layer->vram[dy * layer->w + dx];
        }
#endif
    }
}

/*
void ss_layer_move(Layer* layer, uint16_t x, uint16_t y) {
    uint16_t prevx = layer->x;
    uint16_t prevy = layer->y;
    layer->x = x;
    layer->y = y;
    ss_all_layer_draw_rect(prevx, prevy, prevx + layer->w, prevy + layer->h);
    ss_all_layer_draw_rect(x, y, x + layer->w, y + layer->h);
}
*/

void ss_layer_invalidate(Layer* layer) {
    ss_layer_draw_rect_layer(layer, layer->x, layer->y, layer->x + layer->w,
                             layer->y + layer->h);
}

void ss_layer_update_map(Layer* layer) {
    uint8_t lid = 0;
    if (NULL != layer) {
        lid = layer - ss_layer_mgr->layers;
    }

    for (int i = lid; i < ss_layer_mgr->topLayerIdx; i++) {
        for (int dy = layer->y; dy < layer->y + layer->h; dy++) {
            for (int dx = layer->x; dx < layer->x + layer->w; dx++) {
                ss_layer_mgr->map[dy * WIDTH + dx] = i;
            }
        }
    }
}
