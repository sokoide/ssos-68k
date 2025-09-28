#include <stdint.h>
#include <x68k/iocs.h>

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
    if ((keycode & 0xFFFF) == ESC_SCANCODE) {
        return 0x1B;
    }

    int key = keycode & 0xFF;
    int mod = (keycode >> 8) & 0xFF;
    int shifted = (mod & 0x01) || (mod & 0x02);

    switch (key) {
    case 0x0B:
        return shifted ? '!' : '1';
    case 0x02:
        return shifted ? '"' : '2';
    case 0x03:
        return shifted ? '#' : '3';
    case 0x04:
        return shifted ? '$' : '4';
    case 0x05:
        return shifted ? '%' : '5';
    case 0x06:
        return shifted ? '&' : '6';
    case 0x07:
        return shifted ? '\'' : '7';
    case 0x08:
        return shifted ? '(' : '8';
    case 0x09:
        return shifted ? ')' : '9';
    case 0x0A:
        return shifted ? '*' : '0';
    case 0x14:
        return shifted ? 'Q' : 'q';
    case 0x1A:
        return shifted ? 'W' : 'w';
    case 0x15:
        return shifted ? 'E' : 'e';
    case 0x17:
        return shifted ? 'R' : 'r';
    case 0x1C:
        return shifted ? 'T' : 't';
    case 0x18:
        return shifted ? 'Y' : 'y';
    case 0x0C:
        return shifted ? 'U' : 'u';
    case 0x12:
        return shifted ? 'I' : 'i';
    case 0x13:
        return shifted ? 'O' : 'o';
    case 0x1F:
        return shifted ? 'P' : 'p';
    case 0x1D:
        return shifted ? 'A' : 'a';
    case 0x1B:
        return shifted ? 'S' : 's';
    case 0x16:
        return shifted ? 'D' : 'd';
    case 0x1E:
        return shifted ? 'F' : 'f';
    case 0x11:
        return shifted ? 'G' : 'g';
    case 0x10:
        return shifted ? 'H' : 'h';
    case 0x19:
        return shifted ? 'J' : 'j';
    case 0x28:
        return shifted ? 'K' : 'k';
    case 0x20:
        return shifted ? 'L' : 'l';
    case 0x21:
        return shifted ? 'Z' : 'z';
    case 0x22:
        return shifted ? 'X' : 'x';
    case 0x23:
        return shifted ? 'C' : 'c';
    case 0x24:
        return shifted ? 'V' : 'v';
    case 0x25:
        return shifted ? 'B' : 'b';
    case 0x26:
        return shifted ? 'N' : 'n';
    case 0x27:
        return shifted ? 'M' : 'm';
    case 0x01:
        return '\r';
    case 0x0E:
        return '\b';
    case 0x0F:
        return '\t';
    case 0x39:
        return ' ';
    case 0x2A:
        return shifted ? '|' : '\\';
    case 0x2B:
        return shifted ? '@' : '`';
    case 0x2C:
        return shifted ? '{' : '[';
    case 0x2D:
        return shifted ? '}' : ']';
    case 0x2E:
        return shifted ? '+' : ';';
    case 0x2F:
        return shifted ? '*' : ':';
    case 0x30:
        return shifted ? '<' : ',';
    case 0x31:
        return shifted ? '>' : '.';
    case 0x32:
        return shifted ? '?' : '/';
    default:
        return 0;
    }
}
