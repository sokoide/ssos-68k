#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

// Minimal stubs for required functions
void* ss_mem_alloc(size_t size) {
    return malloc(size);
}

void ss_mem_free(uint32_t addr, size_t size) {
    free((void*)addr);
}

size_t ss_mem_total_bytes(void) {
    return 1024 * 1024;  // 1MB
}

size_t ss_mem_free_bytes(void) {
    return 512 * 1024;   // 512KB
}

// Include QuickDraw headers
#include "../ssos/os/window/quickdraw.h"
#include "../ssos/os/window/quickdraw_monitor.h"

// Mock external variables
const volatile uint32_t ss_timera_counter = 0;
const volatile uint32_t ss_timerd_counter = 0;
const volatile uint32_t ss_context_switch_counter = 0;
const volatile uint32_t ss_save_data_base = 0;
uint32_t global_counter = 0;

// DMA stubs for native testing
void dma_clear(void) { }
void dma_init(uint8_t* addr, int count) { }
void dma_start(void) { }
void dma_wait_completion(void) { }

// XFR_INF stub
struct {
    uint8_t* addr;
    size_t count;
} xfr_inf[1] = { {NULL, 0} };

// Memory info stubs
void* ss_ssos_memory_base = NULL;
size_t ss_ssos_memory_size = 1024 * 1024;

// Memory section info stubs
void ss_get_text(void** base, uint32_t* size) {
    if (base) *base = (void*)0x100000;
    if (size) *size = 65536;
}

void ss_get_data(void** base, uint32_t* size) {
    if (base) *base = (void*)0x200000;
    if (size) *size = 32768;
}

void ss_get_bss(void** base, uint32_t* size) {
    if (base) *base = (void*)0x300000;
    if (size) *size = 16384;
}

// Additional stubs for monitor panel
extern void* ss_ssos_memory_base;
extern size_t ss_ssos_memory_size;

struct {
    int num_free_blocks;
} ss_mem_mgr = { 5 };

// Memory manager stubs (already defined above)
// size_t ss_mem_total_bytes(void) { return 1024 * 1024; }
// size_t ss_mem_free_bytes(void) { return 512 * 1024; }
#include "../ssos/os/kernel/ss_config.h"

static uint8_t test_vram[QD_VRAM_STRIDE * QD_SCREEN_HEIGHT] __attribute__((aligned(4)));
static uint8_t test_font[256 * 16];

int main() {
    printf("Testing monitor panel initialization...\n");
    printf("Test program started\n");

    // Setup QuickDraw system
    printf("Initializing QuickDraw...\n");
    qd_init();
    printf("QuickDraw initialized, is_initialized: %d\n", qd_is_initialized());

    qd_set_vram_buffer(test_vram);
    printf("VRAM buffer set to: %p\n", qd_get_vram_buffer());

    memset(test_font, 0, sizeof(test_font));
    qd_set_font_bitmap(test_font, 8, 16);
    printf("Font bitmap set\n");

    // Debug: Check VRAM buffer before initialization
    const uint8_t* vram_before = qd_get_vram_buffer();
    printf("VRAM buffer before initialization: %p\n", vram_before);
    if (vram_before) {
        printf("First few bytes of VRAM: ");
        for (int i = 0; i < 10; i++) {
            printf("%02x ", vram_before[i]);
        }
        printf("\n");
    }

    // Test monitor panel initialization
    printf("Calling qd_monitor_panel_init()...\n");
    qd_monitor_panel_init();
    printf("qd_monitor_panel_init() completed\n");

    // Debug: Check VRAM buffer after initialization
    const uint8_t* vram_after = qd_get_vram_buffer();
    printf("VRAM buffer after initialization: %p\n", vram_after);
    if (vram_after) {
        printf("First few bytes of VRAM after init: ");
        for (int i = 0; i < 10; i++) {
            printf("%02x ", vram_after[i]);
        }
        printf("\n");
    }

    // Check if VRAM content changed
    if (vram_before && vram_after) {
        bool changed = false;
        for (int i = 0; i < 100; i++) {
            if (vram_before[i] != vram_after[i]) {
                changed = true;
                break;
            }
        }
        printf("VRAM content changed: %s\n", changed ? "YES" : "NO");
    }

    printf("Monitor panel initialized successfully!\n");

    // Check if VRAM was written correctly at the monitor panel location
    const uint8_t* vram = qd_get_vram_buffer();

    if (!vram) {
        printf("ERROR: VRAM buffer is NULL\n");
        return 1;
    }

    // Calculate VRAM offset for the top-left corner of the monitor panel
    size_t panel_x = QD_MONITOR_PANEL_LEFT;
    size_t panel_y = QD_MONITOR_PANEL_TOP;
    size_t vram_offset = panel_y * QD_VRAM_STRIDE + panel_x * 2;

    printf("Panel position: x=%zu, y=%zu\n", panel_x, panel_y);
    printf("VRAM offset: %zu\n", vram_offset);

    // Check the high byte (color value) at the panel position
    uint8_t color_at_panel = vram[vram_offset + 1];
    if (color_at_panel == QD_MONITOR_BODY_COLOR) {
        printf("Background color written correctly\n");
    } else {
        printf("Background color NOT written correctly: expected %d (RED), got %d\n",
                QD_MONITOR_BODY_COLOR, color_at_panel);
        printf("Checking VRAM at offset %zu\n", vram_offset);

        // Check if any part of the panel area has the background color
        int background_pixels = 0;
        for (size_t y = panel_y; y < panel_y + 10; y++) {
            for (size_t x = panel_x; x < panel_x + 10; x++) {
                size_t test_offset = y * QD_VRAM_STRIDE + x * 2;
                uint8_t test_color = vram[test_offset + 1];
                if (test_color == QD_MONITOR_BODY_COLOR) {
                    background_pixels++;
                }
            }
        }
        printf("Found %d background color pixels in panel area\n", background_pixels);

        // Simple test: manually set a pixel and check if it works
        printf("Testing manual pixel write...\n");
        uint8_t* vram_writable = (uint8_t*)qd_get_vram_buffer();
        if (vram_writable) {
            size_t test_pixel_offset = (panel_y + 5) * QD_VRAM_STRIDE + (panel_x + 5) * 2;
            uint8_t old_color = vram_writable[test_pixel_offset + 1];
            vram_writable[test_pixel_offset] = 0x00;
            vram_writable[test_pixel_offset + 1] = 15;  // Bright white
            uint8_t new_color = vram_writable[test_pixel_offset + 1];
            printf("Manual pixel write test: old=%d, new=%d\n", old_color, new_color);
        }
    }

    return 0;
}
