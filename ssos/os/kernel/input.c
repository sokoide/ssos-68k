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

// X68000キーコードをASCIIコードに変換
int x68k_keycode_to_ascii(int keycode) {
    int scancode = keycode & 0x7F;
    int modifiers = (keycode >> 8) & 0xFF;
    int ascii = 0;
    bool is_letter = false;
    char letter_upper = 0;

    bool shift = (modifiers & SS_KB_MOD_SHIFT) != 0;
    bool ctrl = (modifiers & SS_KB_MOD_CTRL) != 0;
    bool caps = (modifiers & SS_KB_MOD_CAPS) != 0;

    if ((keycode & 0xFFFF) == ESC_SCANCODE || scancode == X68K_SC_ESC) {
        return 0x1B;
    }

    switch (scancode) {
    case 0x0B:
        ascii = shift ? '!' : '1';
        break;
    case 0x02:
        ascii = shift ? '"' : '2';
        break;
    case 0x03:
        ascii = shift ? '#' : '3';
        break;
    case 0x04:
        ascii = shift ? '$' : '4';
        break;
    case 0x05:
        ascii = shift ? '%' : '5';
        break;
    case 0x06:
        ascii = shift ? '&' : '6';
        break;
    case 0x07:
        ascii = shift ? '\'' : '7';
        break;
    case 0x08:
        ascii = shift ? '(' : '8';
        break;
    case 0x09:
        ascii = shift ? ')' : '9';
        break;
    case 0x0A:
        ascii = shift ? '*' : '0';
        break;
    case 0x14:
        ascii = 'q';
        letter_upper = 'Q';
        is_letter = true;
        break;
    case 0x1A:
        ascii = 'w';
        letter_upper = 'W';
        is_letter = true;
        break;
    case 0x15:
        ascii = 'e';
        letter_upper = 'E';
        is_letter = true;
        break;
    case 0x17:
        ascii = 'r';
        letter_upper = 'R';
        is_letter = true;
        break;
    case 0x1C:
        ascii = 't';
        letter_upper = 'T';
        is_letter = true;
        break;
    case 0x18:
        ascii = 'y';
        letter_upper = 'Y';
        is_letter = true;
        break;
    case 0x0C:
        ascii = 'u';
        letter_upper = 'U';
        is_letter = true;
        break;
    case 0x12:
        ascii = 'i';
        letter_upper = 'I';
        is_letter = true;
        break;
    case 0x13:
        ascii = 'o';
        letter_upper = 'O';
        is_letter = true;
        break;
    case 0x1F:
        ascii = 'p';
        letter_upper = 'P';
        is_letter = true;
        break;
    case 0x1D:
        ascii = 'a';
        letter_upper = 'A';
        is_letter = true;
        break;
    case 0x1B:
        ascii = 's';
        letter_upper = 'S';
        is_letter = true;
        break;
    case 0x16:
        ascii = 'd';
        letter_upper = 'D';
        is_letter = true;
        break;
    case 0x1E:
        ascii = 'f';
        letter_upper = 'F';
        is_letter = true;
        break;
    case 0x11:
        ascii = 'g';
        letter_upper = 'G';
        is_letter = true;
        break;
    case 0x10:
        ascii = 'h';
        letter_upper = 'H';
        is_letter = true;
        break;
    case 0x19:
        ascii = 'j';
        letter_upper = 'J';
        is_letter = true;
        break;
    case 0x28:
        ascii = 'k';
        letter_upper = 'K';
        is_letter = true;
        break;
    case 0x20:
        ascii = 'l';
        letter_upper = 'L';
        is_letter = true;
        break;
    case 0x21:
        ascii = 'z';
        letter_upper = 'Z';
        is_letter = true;
        break;
    case 0x22:
        ascii = 'x';
        letter_upper = 'X';
        is_letter = true;
        break;
    case 0x23:
        ascii = 'c';
        letter_upper = 'C';
        is_letter = true;
        break;
    case 0x24:
        ascii = 'v';
        letter_upper = 'V';
        is_letter = true;
        break;
    case 0x25:
        ascii = 'b';
        letter_upper = 'B';
        is_letter = true;
        break;
    case 0x26:
        ascii = 'n';
        letter_upper = 'N';
        is_letter = true;
        break;
    case 0x27:
        ascii = 'm';
        letter_upper = 'M';
        is_letter = true;
        break;
    case 0x01:
        ascii = '\r';
        break;
    case 0x0E:
        ascii = '\b';
        break;
    case 0x0F:
        ascii = '\t';
        break;
    case 0x39:
        ascii = ' ';
        break;
    case 0x2A:
        ascii = shift ? '|' : '\\';
        break;
    case 0x2B:
        ascii = shift ? '@' : '`';
        break;
    case 0x2C:
        ascii = shift ? '{' : '[';
        break;
    case 0x2D:
        ascii = shift ? '}' : ']';
        break;
    case 0x2E:
        ascii = shift ? '+' : ';';
        break;
    case 0x2F:
        ascii = shift ? '*' : ':';
        break;
    case 0x30:
        ascii = shift ? '<' : ',';
        break;
    case 0x31:
        ascii = shift ? '>' : '.';
        break;
    case 0x32:
        ascii = shift ? '?' : '/';
        break;
    default:
        return 0;
    }

    if (is_letter) {
        if (ctrl) {
            return letter_upper - 'A' + 1;
        }

        bool uppercase = shift ^ caps;
        ascii = uppercase ? letter_upper : (char)(letter_upper - 'A' + 'a');
        return ascii;
    }

    if (ctrl) {
        return 0;
    }

    return ascii;
}
