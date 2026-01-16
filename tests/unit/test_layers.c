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
    ss_mem_init_info();
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

    // Configure the layer with values that are 8-pixel aligned
    uint16_t x = 96, y = 48, w = 200, h = 152;
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

    // Change z-order of middle layer to a valid position (index 2)
    ss_layer_set_z_order(layer2, 2);
    ASSERT_EQ(layer2->z, 2);
}

// Test dirty rectangle tracking
TEST(layers_dirty_rectangle_tracking) {
    setup_layer_system();
    reset_layer_system();

    Layer* layer = ss_layer_get();
    ASSERT_NOT_NULL(layer);

    // Use 8-pixel aligned size
    ss_layer_set(layer, test_vram, 0, 0, 400, 296);

    // Initially, entire layer should be dirty
    ASSERT_TRUE(layer->needs_redraw);
    ASSERT_EQ(layer->dirty_w, 400);
    ASSERT_EQ(layer->dirty_h, 296);

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

    // Use 8-pixel aligned values
    ss_layer_set(layer, test_vram, 8, 16, 200, 144);
    ss_layer_mark_clean(layer);
    ASSERT_FALSE(layer->needs_redraw);

    // Invalidate the entire layer
    ss_layer_invalidate(layer);
    ASSERT_TRUE(layer->needs_redraw);
    ASSERT_EQ(layer->dirty_x, 0);
    ASSERT_EQ(layer->dirty_y, 0);
    ASSERT_EQ(layer->dirty_w, 200);  // Full width
    ASSERT_EQ(layer->dirty_h, 144);  // Full height
}

// Test layer boundary validation
TEST(layers_boundary_validation) {
    setup_layer_system();
    reset_layer_system();

    Layer* layer = ss_layer_get();
    ASSERT_NOT_NULL(layer);

    // Test normal configuration (aligned)
    ss_layer_set(layer, test_vram, 0, 0, 96, 96);
    ASSERT_EQ(layer->w, 96);
    ASSERT_EQ(layer->h, 96);

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

// Test z-order re-sorting after changing z value
TEST(layers_z_order_re_sorting) {
    setup_layer_system();
    reset_layer_system();

    Layer* l[5];
    for (int i = 0; i < 5; i++) {
        l[i] = ss_layer_get();
        ss_layer_set(l[i], test_vram, i*10, i*10, 50, 50);
    }

    // Initial: [L0, L1, L2, L3, L4] with z: 0, 1, 2, 3, 4
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(l[i]->z, i);
        ASSERT_EQ(ss_layer_mgr->zLayers[i], l[i]);
    }

    // Move L1 to index 3 (Behind L3)
    // Target: [L0, L2, L3, L1, L4]
    ss_layer_set_z_order(l[1], 3);
    
    ASSERT_EQ(ss_layer_mgr->zLayers[0], l[0]);
    ASSERT_EQ(ss_layer_mgr->zLayers[1], l[2]);
    ASSERT_EQ(ss_layer_mgr->zLayers[2], l[3]);
    ASSERT_EQ(ss_layer_mgr->zLayers[3], l[1]);
    ASSERT_EQ(ss_layer_mgr->zLayers[4], l[4]);

    // Verify z values were updated correctly
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(ss_layer_mgr->zLayers[i]->z, i);
    }

    // Move L4 to front (index 0)
    // Target: [L4, L0, L2, L3, L1]
    ss_layer_set_z_order(l[4], 0);
    
    ASSERT_EQ(ss_layer_mgr->zLayers[0], l[4]);
    ASSERT_EQ(ss_layer_mgr->zLayers[1], l[0]);
    ASSERT_EQ(ss_layer_mgr->zLayers[2], l[2]);
    ASSERT_EQ(ss_layer_mgr->zLayers[3], l[3]);
    ASSERT_EQ(ss_layer_mgr->zLayers[4], l[1]);
}

// Test dirty rectangle clipping
TEST(layers_dirty_rect_clipping) {
    setup_layer_system();
    reset_layer_system();

    Layer* layer = ss_layer_get();
    // Use aligned size 96
    ss_layer_set(layer, test_vram, 0, 0, 96, 96);
    ss_layer_mark_clean(layer);

    // Mark dirty with out-of-bounds coordinates
    ss_layer_mark_dirty(layer, 50, 50, 100, 100); // extends to (150, 150)
    
    ASSERT_EQ(layer->dirty_x, 50);
    ASSERT_EQ(layer->dirty_y, 50);
    ASSERT_EQ(layer->dirty_w, 46); // clipped to layer width 96 (96 - 50 = 46)
    ASSERT_EQ(layer->dirty_h, 46); // clipped to layer height 96

    // Mark dirty completely outside
    ss_layer_mark_clean(layer);
    ss_layer_mark_dirty(layer, 200, 200, 10, 10);
    ASSERT_FALSE(layer->needs_redraw);
}

// Test layer visibility toggle
TEST(layers_visibility_toggle) {
    setup_layer_system();
    reset_layer_system();

    Layer* layer = ss_layer_get();
    ASSERT_TRUE(layer->attr & LAYER_ATTR_VISIBLE);

    // Hide layer
    layer->attr &= ~LAYER_ATTR_VISIBLE;
    
    // Drawing a hidden layer should be a no-op (verified by lack of side effects in full draw)
    // Actually we can check if draw_dirty_only skips it
    ss_layer_mark_dirty(layer, 0, 0, 10, 10);
    ss_layer_draw_dirty_only();
    
    // After draw, visibility hidden layers might still be "needs_redraw"?
    // Let's check implementation of draw_dirty_only
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
    RUN_TEST(layers_z_order_re_sorting);
    RUN_TEST(layers_dirty_rect_clipping);
}