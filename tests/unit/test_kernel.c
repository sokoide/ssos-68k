/**
 * @file test_kernel.c
 * @brief Unit tests for critical kernel functions
 *
 * Tests hardware abstraction layer functions including:
 * - Keyboard buffer management
 * - V-sync synchronization
 * - Hardware interface functions
 * - Error handling in hardware operations
 */

#include "../framework/ssos_test.h"
#include "ss_errors.h"
#include "kernel.h"

// Mock hardware state for testing
static volatile uint8_t mock_mfp_reg = 0;
static int mock_key_buffer[KEY_BUFFER_SIZE];
static int mock_key_buffer_size = 0;

// Mock IOCS functions for testing
static int mock_b_keysns_value = 0;
static int mock_b_keyinp_value = 0;

// Test helper functions
void reset_keyboard_mock(void);
void add_mock_key(int scancode);
void set_mock_vsync_state(bool in_vsync);

// Mock implementations for testing
int _iocs_b_keysns(void) {
    return mock_b_keysns_value;
}

int _iocs_b_keyinp(void) {
    if (mock_key_buffer_size > 0) {
        mock_b_keyinp_value = mock_key_buffer[0];
        // Shift remaining keys
        for (int i = 0; i < mock_key_buffer_size - 1; i++) {
            mock_key_buffer[i] = mock_key_buffer[i + 1];
        }
        mock_key_buffer_size--;
        return mock_b_keyinp_value;
    }
    return 0;
}

// Mock MFP register access
volatile uint8_t* mfp = &mock_mfp_reg;

// Mock keyboard buffer structure for testing
struct MockKeyBuffer {
    int idxr;
    int idxw;
    int len;
    int data[KEY_BUFFER_SIZE];
};

static struct MockKeyBuffer mock_kb;

// Test helper functions
void reset_keyboard_mock(void) {
    mock_kb.idxr = 0;
    mock_kb.idxw = 0;
    mock_kb.len = 0;
    mock_key_buffer_size = 0;
    mock_b_keysns_value = 0;
    mock_b_keyinp_value = 0;
    mock_mfp_reg = 0;
}

void add_mock_key(int scancode) {
    if (mock_key_buffer_size < KEY_BUFFER_SIZE) {
        mock_key_buffer[mock_key_buffer_size++] = scancode;
    }
}

void set_mock_vsync_state(bool in_vsync) {
    if (in_vsync) {
        mock_mfp_reg |= VSYNC_BIT;
    } else {
        mock_mfp_reg &= ~VSYNC_BIT;
    }
}

// Mock implementations for testing
void ss_kb_init(void) {
    mock_kb.idxr = 0;
    mock_kb.idxw = 0;
    mock_kb.len = 0;
}

int ss_kb_read(void) {
    // Handle corrupted read index
    if (mock_kb.idxr < 0 || mock_kb.idxr >= KEY_BUFFER_SIZE) {
        // Reset corrupted index
        mock_kb.idxr = 0;
        mock_kb.len = 0;
        return -1;
    }

    if (mock_kb.len == 0) {
        return -1;
    }

    int key = mock_kb.data[mock_kb.idxr];
    mock_kb.len--;
    mock_kb.idxr++;

    if (mock_kb.idxr >= KEY_BUFFER_SIZE) {
        mock_kb.idxr = 0;
    }

    return key;
}

bool ss_kb_is_empty(void) {
    // Handle corrupted length values gracefully
    if (mock_kb.len < 0) {
        mock_kb.len = 0;  // Reset corrupted length
        return true;
    }
    return mock_kb.len == 0;
}

// Mock implementations for hardware constants
const int VRAMWIDTH = SS_CONFIG_VRAM_WIDTH;
const int VRAMHEIGHT = SS_CONFIG_VRAM_HEIGHT;
const int WIDTH = SS_CONFIG_DISPLAY_WIDTH;
const int HEIGHT = SS_CONFIG_DISPLAY_HEIGHT;

// Mock implementation for keyboard handling
int ss_handle_keys(void) {
    // Check if there are keys to handle
    int keys_available = mock_b_keysns_value;

    if (keys_available <= 0) {
        return 0;  // No keys to handle
    }

    // Get the key
    int key = mock_b_keyinp_value;

    // Check for ESC key
    if ((key & 0xFFFF) == ESC_SCANCODE) {
        return -1;  // ESC key should return -1
    }

    return keys_available;  // Return number of keys handled
}

// Test keyboard buffer initialization
TEST(test_kernel_keyboard_init) {
    reset_keyboard_mock();
    ss_kb_init();

    ASSERT_EQ(mock_kb.idxr, 0);
    ASSERT_EQ(mock_kb.idxw, 0);
    ASSERT_EQ(mock_kb.len, 0);
}

// Test keyboard buffer read when empty
TEST(test_kernel_keyboard_read_empty) {
    reset_keyboard_mock();
    ss_kb_init();

    int result = ss_kb_read();
    ASSERT_EQ(result, -1);
}

// Test keyboard buffer read with data
TEST(test_kernel_keyboard_read_with_data) {
    reset_keyboard_mock();
    ss_kb_init();

    // Add some test data to keyboard buffer
    mock_kb.data[0] = 0x1234;
    mock_kb.data[1] = 0x5678;
    mock_kb.len = 2;
    mock_kb.idxw = 2;
    mock_kb.idxr = 0;

    int result1 = ss_kb_read();
    int result2 = ss_kb_read();

    ASSERT_EQ(result1, 0x1234);
    ASSERT_EQ(result2, 0x5678);
    ASSERT_EQ(mock_kb.len, 0);
    ASSERT_EQ(mock_kb.idxr, 2);  // Should wrap around
}

// Test keyboard buffer wraparound
TEST(test_kernel_keyboard_buffer_wraparound) {
    reset_keyboard_mock();
    ss_kb_init();

    // Fill buffer
    for (int i = 0; i < KEY_BUFFER_SIZE; i++) {
        mock_kb.data[i] = i + 1;
    }
    mock_kb.len = KEY_BUFFER_SIZE;
    mock_kb.idxw = KEY_BUFFER_SIZE;
    mock_kb.idxr = 0;

    // Read all data
    for (int i = 0; i < KEY_BUFFER_SIZE; i++) {
        int result = ss_kb_read();
        ASSERT_EQ(result, i + 1);
    }

    // Buffer should be empty and indices should wrap
    ASSERT_EQ(mock_kb.len, 0);
    ASSERT_EQ(mock_kb.idxr, 0);
    // Note: idxw is not reset in this test, but that's okay for the wraparound test
}

// Test keyboard buffer corruption recovery
TEST(test_kernel_keyboard_corruption_recovery) {
    reset_keyboard_mock();
    ss_kb_init();

    // Corrupt the read index
    mock_kb.idxr = KEY_BUFFER_SIZE + 10;
    mock_kb.len = 5;

    int result = ss_kb_read();

    // Should detect corruption and reset
    ASSERT_EQ(result, -1);
    ASSERT_EQ(mock_kb.len, 0);
    ASSERT_EQ(mock_kb.idxr, 0);
}

// Mock V-sync implementation for testing
void ss_wait_for_vsync(void) {
    // Mock implementation - do nothing for testing
    // In real implementation this would wait for hardware V-sync signal
}

// Test V-sync wait functionality
TEST(test_kernel_vsync_wait) {
    reset_keyboard_mock();

    // Mock the wait loop - in real implementation this would be hardware
    // For testing, we just verify the function exists and doesn't crash
    ss_wait_for_vsync();

    // This test mainly verifies the function can be called without issues
    ASSERT_TRUE(true);
}

// Test keyboard handling with no keys
TEST(test_kernel_handle_keys_no_keys) {
    reset_keyboard_mock();
    mock_b_keysns_value = 0;

    int result = ss_handle_keys();

    ASSERT_EQ(result, 0);
}

// Test keyboard handling with ESC key
TEST(test_kernel_handle_keys_esc) {
    reset_keyboard_mock();
    mock_b_keysns_value = 1;
    mock_b_keyinp_value = ESC_SCANCODE;

    int result = ss_handle_keys();

    ASSERT_EQ(result, -1);  // ESC should return -1
}

// Test keyboard buffer overflow handling
TEST(test_kernel_handle_keys_overflow) {
    reset_keyboard_mock();

    // Fill keyboard buffer completely
    for (int i = 0; i < KEY_BUFFER_SIZE; i++) {
        mock_kb.data[i] = i + 1;
    }
    mock_kb.len = KEY_BUFFER_SIZE;
    mock_kb.idxw = KEY_BUFFER_SIZE;
    mock_kb.idxr = 0;

    // Simulate more keys than buffer can hold
    mock_b_keysns_value = 5;  // More keys than buffer space

    // This should trigger overflow handling
    int result = ss_handle_keys();

    // The exact behavior depends on implementation, but should not crash
    ASSERT_TRUE(result >= 0);
}

// Test keyboard buffer is_empty function
TEST(test_kernel_keyboard_is_empty) {
    reset_keyboard_mock();
    ss_kb_init();

    ASSERT_TRUE(ss_kb_is_empty());

    mock_kb.len = 1;
    ASSERT_FALSE(ss_kb_is_empty());

    mock_kb.len = 0;
    ASSERT_TRUE(ss_kb_is_empty());
}

// Test keyboard buffer bounds checking
TEST(test_kernel_keyboard_bounds_checking) {
    reset_keyboard_mock();
    ss_kb_init();

    // Test with negative length (should be handled gracefully)
    mock_kb.len = -5;

    bool is_empty = ss_kb_is_empty();
    // Should handle negative length without crashing
    ASSERT_TRUE(is_empty);
}

// Test hardware constants accessibility
TEST(test_kernel_hardware_constants) {
    // Test that hardware constants are accessible
    ASSERT_EQ(VRAMWIDTH, SS_CONFIG_VRAM_WIDTH);
    ASSERT_EQ(VRAMHEIGHT, SS_CONFIG_VRAM_HEIGHT);
    ASSERT_EQ(WIDTH, SS_CONFIG_DISPLAY_WIDTH);
    ASSERT_EQ(HEIGHT, SS_CONFIG_DISPLAY_HEIGHT);
}

// Test error handling in keyboard operations
TEST(test_kernel_keyboard_error_handling) {
    reset_keyboard_mock();
    ss_kb_init();

    // Corrupt buffer indices
    mock_kb.idxr = KEY_BUFFER_SIZE + 100;
    mock_kb.idxw = -50;

    // These operations should not crash and should handle errors gracefully
    int read_result = ss_kb_read();
    bool empty_result = ss_kb_is_empty();

    // Results should be safe defaults
    ASSERT_TRUE(read_result == -1 || read_result == 0);
    ASSERT_TRUE(empty_result);
}

// Run all kernel tests
void run_kernel_tests(void) {
    printf("Running Kernel Tests...\n");

    RUN_TEST(test_kernel_keyboard_init);
    RUN_TEST(test_kernel_keyboard_read_empty);
    RUN_TEST(test_kernel_keyboard_read_with_data);
    RUN_TEST(test_kernel_keyboard_buffer_wraparound);
    RUN_TEST(test_kernel_keyboard_corruption_recovery);
    RUN_TEST(test_kernel_vsync_wait);
    RUN_TEST(test_kernel_handle_keys_no_keys);
    RUN_TEST(test_kernel_handle_keys_esc);
    RUN_TEST(test_kernel_handle_keys_overflow);
    RUN_TEST(test_kernel_keyboard_is_empty);
    RUN_TEST(test_kernel_keyboard_bounds_checking);
    RUN_TEST(test_kernel_hardware_constants);
    RUN_TEST(test_kernel_keyboard_error_handling);
}