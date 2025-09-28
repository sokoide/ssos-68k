#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma warning disable format
#include <x68k/iocs.h>
#pragma warning restore format

extern void ssosmain();
extern void set_interrupts();
extern void restore_interrupts();

char local_info[256];
void* local_ssos_memory_base;
// Simulate real keyboard hardware for LOCAL_MODE
// These functions will be called by the new MFP-based interrupt handler

static uint8_t simulated_key_buffer[64] = {0};
static int simulated_key_head = 0;
static int simulated_key_tail = 0;
static uint8_t last_scancode = 0;

// Add a simulated keypress (for testing)
void simulate_keypress(uint8_t scancode) {
    int next_head = (simulated_key_head + 1) % 64;
    if (next_head != simulated_key_tail) {  // Buffer not full
        simulated_key_buffer[simulated_key_head] = scancode;
        simulated_key_head = next_head;
    }
}

uint8_t ss_keyboard_hw_status(void) {
    // Return "ready" if we have simulated keys in buffer
    if (simulated_key_head != simulated_key_tail) {
        return 0x01;  // Data ready
    }
    return 0x00;  // No data
}

uint8_t ss_keyboard_hw_read_data(void) {
    // Return simulated key from buffer
    if (simulated_key_head != simulated_key_tail) {
        last_scancode = simulated_key_buffer[simulated_key_tail];
        simulated_key_tail = (simulated_key_tail + 1) % 64;
        return last_scancode;
    }
    return 0;  // No data
}

void ss_keyboard_hw_ack(void) {
    // Simulate MFP interrupt acknowledge
    // In real hardware, this would write to 0xe88019
}

// Declare assembly functions
extern int ss_kb_read_raw(void);
extern void ss_buffer_raw_key(void);

// Test function to simulate typing "HELLO"
void test_keyboard_input(void) {
    // Simulate typing "HELLO" with X68000 scancodes
    // H scancode: 0x23
    // E scancode: 0x12
    // L scancode: 0x26
    // O scancode: 0x18
    simulate_keypress(0x23);  // H
    simulate_keypress(0x12);  // E
    simulate_keypress(0x26);  // L
    simulate_keypress(0x26);  // L
    simulate_keypress(0x18);  // O
}
uint32_t local_ssos_memory_size = 10 * 1024 * 1024;
uint32_t local_text_size;
uint32_t local_data_size;
uint32_t local_bss_size;

int main(int argc, char** argv) {
    int ssp = _iocs_b_super(0);  // enter supervisor mode

    set_interrupts();

    // get local info
    strcpy(local_info, argv[0]);

    FILE* fp = fopen(argv[0], "rb");
    fseek(fp, 0xc, SEEK_SET);
    fread(&local_text_size, 4, 1, fp);
    fread(&local_data_size, 4, 1, fp);
    fread(&local_bss_size, 4, 1, fp);
    fclose(fp);
    sprintf(local_info, "text size: %9d\ndata size: %9d\nbss size:  %9d",
            local_text_size, local_data_size, local_bss_size);

    local_ssos_memory_base = malloc(local_ssos_memory_size);
    assert(local_ssos_memory_base);

    // Test keyboard input simulation
    test_keyboard_input();
    
    ssosmain();

    if (NULL != local_ssos_memory_base)
        free(local_ssos_memory_base);

    restore_interrupts();

    _iocs_b_super(ssp);  // leave supervisor mode
    return 0;
}
