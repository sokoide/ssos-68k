#pragma once

#include <stdbool.h>
#include "layer.h"

typedef enum {
    SS_LAYER_BACKEND_LEGACY = 0,
    SS_LAYER_BACKEND_QUICKDRAW = 1,
    SS_LAYER_BACKEND_SIMPLE = 2
} SsLayerBackend;

void ss_layer_compat_select(SsLayerBackend backend);
SsLayerBackend ss_layer_compat_active_backend(void);
bool ss_layer_compat_uses_quickdraw(void);

void ss_layer_compat_on_dirty_marked(Layer* layer);
void ss_layer_compat_on_layer_cleaned(Layer* layer);

Layer* get_layer_1();
Layer* get_layer_2();
Layer* get_layer_3();
void update_layer_2(Layer* l);
void update_layer_3(Layer* l);