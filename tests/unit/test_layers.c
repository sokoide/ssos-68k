#include "../framework/ssos_test.h"

// Include SSOS layer management system
#include "layer.h"
#include "ss_config.h"
#include "memory.h"

// External declarations for layer system
extern LayerMgr* ss_layer_mgr;

// Mock VRAM buffer for testing
static uint8_t test_vram[1024 * 1024];  // 1MB test VRAM

// Test helper: Initialize layer system
static void setup_layer_system(void) {
    ss_mem_init();
    ss_layer_init();
}

// Test helper: Reset layer system state
static void reset_layer_system(void) {
    if (ss_layer_mgr) {
        // Reset layer manager state
        ss_layer_mgr->topLayerIdx = 0;

        // Clear all layers
        for (int i = 0; i < MAX_LAYERS; i++) {
            ss_layer_mgr->layers[i].attr = 0;  // Mark as unused
            ss_layer_mgr->layers[i].x = 0;
            ss_layer_mgr->layers[i].y = 0;
            ss_layer_mgr->layers[i].w = 0;
            ss_layer_mgr->layers[i].h = 0;
            ss_layer_mgr->layers[i].z = 0;
            ss_layer_mgr->layers[i].vram = NULL;
            ss_layer_mgr->layers[i].needs_redraw = 0;
            ss_layer_mgr->zLayers[i] = NULL;
        }
    }
}

// Test basic layer initialization
TEST(layers_initialization_basic) {
    setup_layer_system();

    ASSERT_NOT_NULL(ss_layer_mgr);
    ASSERT_EQ(ss_layer_mgr->topLayerIdx, 0);

    // All layers should initially be unused
    for (int i = 0; i < MAX_LAYERS; i++) {
        ASSERT_FALSE(ss_layer_mgr->layers[i].attr & LAYER_ATTR_USED);
        ASSERT_NULL(ss_layer_mgr->zLayers[i]);
    }
}

// Test layer allocation
TEST(layers_allocation_basic) {
    setup_layer_system();
    reset_layer_system();

    Layer* layer1 = ss_layer_get();
    ASSERT_NOT_NULL(layer1);
    ASSERT_TRUE(layer1->attr & LAYER_ATTR_USED);
    ASSERT_TRUE(layer1->attr & LAYER_ATTR_VISIBLE);
    ASSERT_EQ(layer1->z, 0);  // First layer gets z=0
    ASSERT_EQ(ss_layer_mgr->topLayerIdx, 1);

    Layer* layer2 = ss_layer_get();
    ASSERT_NOT_NULL(layer2);
    ASSERT_NEQ(layer1, layer2);  // Different layer objects
    ASSERT_EQ(layer2->z, 1);     // Second layer gets z=1
    ASSERT_EQ(ss_layer_mgr->topLayerIdx, 2);
}

// Test layer resource exhaustion
TEST(layers_allocation_exhaustion) {
    setup_layer_system();
    reset_layer_system();

    Layer* layers[MAX_LAYERS + 1];
    int allocated_count = 0;

    // Allocate all available layers
    for (int i = 0; i < MAX_LAYERS + 1; i++) {
        layers[i] = ss_layer_get();
        if (layers[i] != NULL) {
            allocated_count++;
        } else {
            break;
        }
    }

    // Should allocate exactly MAX_LAYERS
    ASSERT_EQ(allocated_count, MAX_LAYERS);
    ASSERT_NULL(layers[MAX_LAYERS]);  // Last allocation should fail
}

// Test layer configuration
TEST(layers_configuration_basic) {
    setup_layer_system();
    reset_layer_system();

    Layer* layer = ss_layer_get();
    ASSERT_NOT_NULL(layer);

    // Configure the layer
    uint16_t x = 100, y = 50, w = 200, h = 150;
    ss_layer_set(layer, test_vram, x, y, w, h);

    ASSERT_EQ(layer->x, x);
    ASSERT_EQ(layer->y, y);
    ASSERT_EQ(layer->w, w);
    ASSERT_EQ(layer->h, h);
    ASSERT_EQ(layer->vram, test_vram);

    // Dirty rectangle should be set to full layer initially
    ASSERT_EQ(layer->dirty_w, w);
    ASSERT_EQ(layer->dirty_h, h);
    ASSERT_TRUE(layer->needs_redraw);
}

// Test z-order management
TEST(layers_z_order_management) {
    setup_layer_system();
    reset_layer_system();

    // Create multiple layers
    Layer* layer1 = ss_layer_get();
    Layer* layer2 = ss_layer_get();
    Layer* layer3 = ss_layer_get();

    ASSERT_NOT_NULL(layer1);
    ASSERT_NOT_NULL(layer2);
    ASSERT_NOT_NULL(layer3);

    // Initial z-order should be creation order
    ASSERT_EQ(layer1->z, 0);
    ASSERT_EQ(layer2->z, 1);
    ASSERT_EQ(layer3->z, 2);

    // Z-order array should be populated correctly
    ASSERT_EQ(ss_layer_mgr->zLayers[0], layer1);
    ASSERT_EQ(ss_layer_mgr->zLayers[1], layer2);
    ASSERT_EQ(ss_layer_mgr->zLayers[2], layer3);

    // Change z-order of middle layer
    ss_layer_set_z_order(layer2, 5);
    ASSERT_EQ(layer2->z, 5);
}

// Test dirty rectangle tracking
TEST(layers_dirty_rectangle_tracking) {
    setup_layer_system();
    reset_layer_system();

    Layer* layer = ss_layer_get();
    ASSERT_NOT_NULL(layer);

    ss_layer_set(layer, test_vram, 0, 0, 400, 300);

    // Initially, entire layer should be dirty
    ASSERT_TRUE(layer->needs_redraw);
    ASSERT_EQ(layer->dirty_w, 400);
    ASSERT_EQ(layer->dirty_h, 300);

    // Mark layer as clean
    ss_layer_mark_clean(layer);
    ASSERT_FALSE(layer->needs_redraw);

    // Mark a specific area as dirty
    ss_layer_mark_dirty(layer, 50, 60, 100, 80);
    ASSERT_TRUE(layer->needs_redraw);
    ASSERT_EQ(layer->dirty_x, 50);
    ASSERT_EQ(layer->dirty_y, 60);
    ASSERT_EQ(layer->dirty_w, 100);
    ASSERT_EQ(layer->dirty_h, 80);
}

// Test layer invalidation
TEST(layers_invalidation) {
    setup_layer_system();
    reset_layer_system();

    Layer* layer = ss_layer_get();
    ASSERT_NOT_NULL(layer);

    ss_layer_set(layer, test_vram, 10, 20, 200, 150);
    ss_layer_mark_clean(layer);
    ASSERT_FALSE(layer->needs_redraw);

    // Invalidate the entire layer
    ss_layer_invalidate(layer);
    ASSERT_TRUE(layer->needs_redraw);
    ASSERT_EQ(layer->dirty_x, 0);
    ASSERT_EQ(layer->dirty_y, 0);
    ASSERT_EQ(layer->dirty_w, 200);  // Full width
    ASSERT_EQ(layer->dirty_h, 150);  // Full height
}

// Test layer boundary validation
TEST(layers_boundary_validation) {
    setup_layer_system();
    reset_layer_system();

    Layer* layer = ss_layer_get();
    ASSERT_NOT_NULL(layer);

    // Test normal configuration
    ss_layer_set(layer, test_vram, 0, 0, 100, 100);
    ASSERT_EQ(layer->w, 100);
    ASSERT_EQ(layer->h, 100);

    // Test maximum size configuration
    ss_layer_set(layer, test_vram, 0, 0, SS_CONFIG_VRAM_WIDTH, SS_CONFIG_VRAM_HEIGHT);
    ASSERT_EQ(layer->w, SS_CONFIG_VRAM_WIDTH);
    ASSERT_EQ(layer->h, SS_CONFIG_VRAM_HEIGHT);
}

// Test multiple layer interaction
TEST(layers_multiple_layer_interaction) {
    setup_layer_system();
    reset_layer_system();

    // Create overlapping layers
    Layer* background = ss_layer_get();
    Layer* foreground = ss_layer_get();

    ASSERT_NOT_NULL(background);
    ASSERT_NOT_NULL(foreground);

    // Configure layers
    ss_layer_set(background, test_vram, 0, 0, 400, 300);
    ss_layer_set(foreground, test_vram + 1000, 50, 50, 200, 150);

    // Background should be at z=0, foreground at z=1
    ASSERT_EQ(background->z, 0);
    ASSERT_EQ(foreground->z, 1);

    // Both should be marked for redraw initially
    ASSERT_TRUE(background->needs_redraw);
    ASSERT_TRUE(foreground->needs_redraw);

    // Clean both layers
    ss_layer_mark_clean(background);
    ss_layer_mark_clean(foreground);

    // Mark overlapping area as dirty on background
    ss_layer_mark_dirty(background, 25, 25, 100, 100);
    ASSERT_TRUE(background->needs_redraw);
    ASSERT_FALSE(foreground->needs_redraw);  // Foreground unaffected
}

// Test layer memory management
TEST(layers_memory_management) {
    setup_layer_system();
    reset_layer_system();

    Layer* layer = ss_layer_get();
    ASSERT_NOT_NULL(layer);

    // Test with different VRAM pointers
    uint8_t* vram1 = test_vram;
    uint8_t* vram2 = test_vram + 10000;

    ss_layer_set(layer, vram1, 0, 0, 100, 100);
    ASSERT_EQ(layer->vram, vram1);

    // Change VRAM buffer
    ss_layer_set(layer, vram2, 0, 0, 100, 100);
    ASSERT_EQ(layer->vram, vram2);

    // Layer should be marked for redraw when VRAM changes
    ASSERT_TRUE(layer->needs_redraw);
}

// Run all layer tests
void run_layer_tests(void) {
    RUN_TEST(layers_initialization_basic);
    RUN_TEST(layers_allocation_basic);
    RUN_TEST(layers_allocation_exhaustion);
    RUN_TEST(layers_configuration_basic);
    RUN_TEST(layers_z_order_management);
    RUN_TEST(layers_dirty_rectangle_tracking);
    RUN_TEST(layers_invalidation);
    RUN_TEST(layers_boundary_validation);
    RUN_TEST(layers_multiple_layer_interaction);
    RUN_TEST(layers_memory_management);
}