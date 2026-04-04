#pragma once
#include "layer.h"

typedef struct {
    bool is_dragging;
    Layer* dragged_layer;
    uint16_t drag_offset_x, drag_offset_y;
    int16_t drag_frame_x, drag_frame_y;
    int16_t drag_old_frame_x, drag_old_frame_y;
    bool prev_lclick;  // Previous mouse left click state
} DragState;

extern DragState g_drag;

Layer* get_layer_1();
Layer* get_layer_2();
Layer* get_layer_3();
void update_layer_2(Layer* l);
void update_layer_3(Layer* l);

// Lightweight mouse processing for drag frame updates (called during drag)
void ss_drag_update_frame(void);