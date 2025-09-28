#include "kernel.h"

#include <x68k/iocs.h>

#include "ss_errors.h"

// Define constants for external linkage - needed for code that takes addresses
const int VRAMWIDTH = SS_CONFIG_VRAM_WIDTH;
const int VRAMHEIGHT = SS_CONFIG_VRAM_HEIGHT;
const int WIDTH = SS_CONFIG_DISPLAY_WIDTH;
const int HEIGHT = SS_CONFIG_DISPLAY_HEIGHT;
const uint16_t color_fg = SS_CONFIG_COLOR_FOREGROUND;
const uint16_t color_bg = SS_CONFIG_COLOR_BACKGROUND;
const uint16_t color_tb = SS_CONFIG_COLOR_TASKBAR;

// Constants now defined as macros in kernel.h via ss_config.h
// These declarations are no longer needed as they're now macros

volatile uint8_t* mfp = (volatile uint8_t*)MFP_ADDRESS;

struct KeyBuffer ss_kb;

static bool ss_kb_enqueue(int scancode) {
    if (ss_kb.idxw < 0 || ss_kb.idxw >= KEY_BUFFER_SIZE || ss_kb.idxr < 0 ||
        ss_kb.idxr >= KEY_BUFFER_SIZE) {
        ss_kb.idxw = 0;
        ss_kb.idxr = 0;
        ss_kb.len = 0;
        ss_set_error(SS_ERROR_OUT_OF_BOUNDS, SS_SEVERITY_WARNING, __func__,
                     __FILE__, __LINE__,
                     "Keyboard buffer index corrupted, resetting");
    }

    if (ss_kb.len >= KEY_BUFFER_SIZE) {
        return false;
    }

    ss_kb.data[ss_kb.idxw] = scancode;
    ss_kb.idxw++;
    if (ss_kb.idxw >= KEY_BUFFER_SIZE) {
        ss_kb.idxw = 0;
    }
    ss_kb.len++;
    return true;
}

void ss_wait_for_vsync() {
    // if it's vsync, wait for display period
    while (!((*mfp) & VSYNC_BIT));
    // wait for vsync
    while ((*mfp) & VSYNC_BIT);
}

int ss_handle_keys() {
    int handled_keys = 0;
    int dropped_keys = 0;
    bool esc_seen = false;

    while (_iocs_b_keysns() > 0) {
        int scancode = _iocs_b_keyinp();
        handled_keys++;

        if (!ss_kb_enqueue(scancode)) {
            dropped_keys++;
        }

        if ((scancode & 0xFFFF) == ESC_SCANCODE) {
            esc_seen = true;
        }
    }

    if (dropped_keys > 0) {
        ss_set_error(SS_ERROR_OUT_OF_BOUNDS, SS_SEVERITY_WARNING, __func__,
                     __FILE__, __LINE__,
                     "Keyboard buffer overflow - keys dropped");
    }

    if (esc_seen) {
        ss_set_error(SS_ERROR_SYSTEM_ERROR, SS_SEVERITY_INFO, __func__,
                     __FILE__, __LINE__,
                     "ESC key pressed - system shutdown requested");
        return -1;
    }

    return handled_keys;
}

void ss_kb_init() {
    ss_kb.idxr = 0;
    ss_kb.idxw = 0;
    ss_kb.len = 0;
}

int ss_kb_read() {
    // Check if buffer is empty
    if (ss_kb.len == 0) {
        return -1;
    }

    // Validate buffer state before reading
    if (ss_kb.idxr < 0 || ss_kb.idxr >= KEY_BUFFER_SIZE) {
        // Reset corrupted index
        ss_kb.idxr = 0;
        ss_kb.len = 0;
        ss_set_error(SS_ERROR_OUT_OF_BOUNDS, SS_SEVERITY_ERROR, __func__,
                     __FILE__, __LINE__,
                     "Keyboard buffer read index corrupted");
        return -1;
    }

    // Safe read from buffer
    int key = ss_kb.data[ss_kb.idxr];
    ss_kb.len--;
    ss_kb.idxr++;

    // Wrap around buffer index
    if (ss_kb.idxr >= KEY_BUFFER_SIZE) {
        ss_kb.idxr = 0;
    }

    return key;
}

bool ss_kb_is_empty() {
    return ss_kb.len == 0;
}
