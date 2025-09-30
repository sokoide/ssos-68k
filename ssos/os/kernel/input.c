#include <stdbool.h>
#include <stdint.h>
#include <x68k/iocs.h>

#include "kernel.h"
#include "ss_config.h"

// IOCSコールの実装（インラインアセンブリ）
int _iocs_b_keyinp(void) {
    int result;
    register int d0 asm("d0");
    d0 = 0x00;  // _B_KEYINP
    asm volatile("trap #15\n" : "=d"(d0) : "0"(d0));
    result = d0;
    return result;
}

int _iocs_b_keysns(void) {
    int result;
    register int d0 asm("d0");
    d0 = 0x01;  // _B_KEYSNS
    asm volatile("trap #15\n" : "=d"(d0) : "0"(d0));
    result = d0;
    return result;
}

int _iocs_b_coninp(void) {
    int result;
    register int d0 asm("d0");
    d0 = 0x02;  // _B_CONINP
    asm volatile("trap #15\n" : "=d"(d0) : "0"(d0));
    result = d0;
    return result & 0xFF;  // ASCIIコードが返る
}

// Key mapping types
typedef enum {
    KEY_TYPE_NONE,
    KEY_TYPE_FIXED,
    KEY_TYPE_LETTER,
    KEY_TYPE_NUMBER,
    KEY_TYPE_SYMBOL
} KeyType;

// Key mapping entry
typedef struct {
    uint8_t scancode;
    KeyType type;
    union {
        struct {
            uint8_t ascii;
        } fixed;
        struct {
            char upper;
            char lower;
        } letter;
        struct {
            char number;
            char shift_symbol;
        } number;
        struct {
            char normal;
            char shifted;
        } symbol;
    } value;
} KeyMapping;

// Key mapping table - organized by scancode
static const KeyMapping key_map[] = {
    // Special keys
    {0x01, KEY_TYPE_FIXED, .value.fixed = {0x1B}},  // ESC

    // Number keys
    {0x02, KEY_TYPE_NUMBER, .value.number = {'1', '!'}},
    {0x03, KEY_TYPE_NUMBER, .value.number = {'2', '@'}},
    {0x04, KEY_TYPE_NUMBER, .value.number = {'3', '#'}},
    {0x05, KEY_TYPE_NUMBER, .value.number = {'4', '$'}},
    {0x06, KEY_TYPE_NUMBER, .value.number = {'5', '%'}},
    {0x07, KEY_TYPE_NUMBER, .value.number = {'6', '^'}},
    {0x08, KEY_TYPE_NUMBER, .value.number = {'7', '&'}},
    {0x09, KEY_TYPE_NUMBER, .value.number = {'8', '*'}},
    {0x0a, KEY_TYPE_NUMBER, .value.number = {'9', '('}},
    {0x0b, KEY_TYPE_NUMBER, .value.number = {'0', ')'}},

    // Letter keys - top row
    {0x11, KEY_TYPE_LETTER, .value.letter = {'Q', 'q'}},
    {0x12, KEY_TYPE_LETTER, .value.letter = {'W', 'w'}},
    {0x13, KEY_TYPE_LETTER, .value.letter = {'E', 'e'}},
    {0x14, KEY_TYPE_LETTER, .value.letter = {'R', 'r'}},
    {0x15, KEY_TYPE_LETTER, .value.letter = {'T', 't'}},
    {0x16, KEY_TYPE_LETTER, .value.letter = {'Y', 'y'}},
    {0x17, KEY_TYPE_LETTER, .value.letter = {'U', 'u'}},
    {0x18, KEY_TYPE_LETTER, .value.letter = {'I', 'i'}},
    {0x19, KEY_TYPE_LETTER, .value.letter = {'O', 'o'}},
    {0x1a, KEY_TYPE_LETTER, .value.letter = {'P', 'p'}},

    // Letter keys - home row
    {0x1e, KEY_TYPE_LETTER, .value.letter = {'A', 'a'}},
    {0x1f, KEY_TYPE_LETTER, .value.letter = {'S', 's'}},
    {0x20, KEY_TYPE_LETTER, .value.letter = {'D', 'd'}},
    {0x21, KEY_TYPE_LETTER, .value.letter = {'F', 'f'}},
    {0x22, KEY_TYPE_LETTER, .value.letter = {'G', 'g'}},
    {0x23, KEY_TYPE_LETTER, .value.letter = {'H', 'h'}},
    {0x24, KEY_TYPE_LETTER, .value.letter = {'J', 'j'}},
    {0x25, KEY_TYPE_LETTER, .value.letter = {'K', 'k'}},
    {0x26, KEY_TYPE_LETTER, .value.letter = {'L', 'l'}},

    // Letter keys - bottom row
    {0x2a, KEY_TYPE_LETTER, .value.letter = {'Z', 'z'}},
    {0x2b, KEY_TYPE_LETTER, .value.letter = {'X', 'x'}},
    {0x2c, KEY_TYPE_LETTER, .value.letter = {'C', 'c'}},
    {0x2d, KEY_TYPE_LETTER, .value.letter = {'V', 'v'}},
    {0x2e, KEY_TYPE_LETTER, .value.letter = {'B', 'b'}},
    {0x2f, KEY_TYPE_LETTER, .value.letter = {'N', 'n'}},
    {0x30, KEY_TYPE_LETTER, .value.letter = {'M', 'm'}},

    // Special keys
    {0x0f, KEY_TYPE_FIXED, .value.fixed = {'\b'}},  // Backspace
    {0x10, KEY_TYPE_FIXED, .value.fixed = {'\t'}},  // Tab
    {0x1d, KEY_TYPE_FIXED, .value.fixed = {'\n'}},  // Enter
    {0x35, KEY_TYPE_FIXED, .value.fixed = {' '}},   // Space

    // Symbol keys
    {0x0c, KEY_TYPE_SYMBOL, .value.symbol = {'-', '_'}},
    {0x0d, KEY_TYPE_SYMBOL, .value.symbol = {'+', '+'}},
    {0x4a, KEY_TYPE_SYMBOL, .value.symbol = {'=', '='}},
    {0x1b, KEY_TYPE_SYMBOL, .value.symbol = {'[', '{'}},
    {0x1c, KEY_TYPE_SYMBOL, .value.symbol = {']', '}'}},
    {0x0e, KEY_TYPE_SYMBOL, .value.symbol = {'\\', '|'}},
    {0x29, KEY_TYPE_SYMBOL, .value.symbol = {'`', '~'}},

    {0x27, KEY_TYPE_SYMBOL, .value.symbol = {';', ':'}},
    {0x28, KEY_TYPE_SYMBOL, .value.symbol = {'\'', '"'}},
    {0x31, KEY_TYPE_SYMBOL, .value.symbol = {',', '<'}},
    {0x32, KEY_TYPE_SYMBOL, .value.symbol = {'.', '>'}},
    {0x33, KEY_TYPE_SYMBOL, .value.symbol = {'/', '?'}},
};

// Helper function to find key mapping by scancode
static const KeyMapping* find_key_mapping(uint8_t scancode) {
    for (size_t i = 0; i < sizeof(key_map) / sizeof(key_map[0]); i++) {
        if (key_map[i].scancode == scancode) {
            return &key_map[i];
        }
    }
    return NULL;
}

// X68000キーコードをASCIIコードに変換
int x68k_keycode_to_ascii(int keycode) {
    int scancode = keycode & 0x7F;
    int modifiers = (keycode >> 8) & 0xFF;

    bool shift = (modifiers & SS_KB_MOD_SHIFT) != 0;
    bool ctrl = (modifiers & SS_KB_MOD_CTRL) != 0;
    bool caps = (modifiers & SS_KB_MOD_CAPS) != 0;

    // Handle ESC scan code
    if ((keycode & 0xFFFF) == ESC_SCANCODE || scancode == X68K_SC_ESC) {
        return 0x1B;
    }

    const KeyMapping* mapping = find_key_mapping(scancode);
    if (!mapping) {
        return 0;
    }

    int ascii = 0;

    switch (mapping->type) {
    case KEY_TYPE_FIXED:
        ascii = mapping->value.fixed.ascii;
        break;

    case KEY_TYPE_LETTER:
        if (ctrl) {
            // Ctrl+A = 1, Ctrl+B = 2, etc.
            return mapping->value.letter.upper - 'A' + 1;
        }
        {
            bool uppercase = shift ^ caps;
            ascii = uppercase ? mapping->value.letter.upper
                              : mapping->value.letter.lower;
        }
        break;

    case KEY_TYPE_NUMBER:
        ascii = shift ? mapping->value.number.shift_symbol
                      : mapping->value.number.number;
        break;

    case KEY_TYPE_SYMBOL:
        ascii = shift ? mapping->value.symbol.shifted
                      : mapping->value.symbol.normal;
        break;

    case KEY_TYPE_NONE:
    default:
        return 0;
    }

    // For non-letter keys, Ctrl usually returns 0 (or special handling)
    if (ctrl && mapping->type != KEY_TYPE_LETTER) {
        return 0;
    }

    return ascii;
}