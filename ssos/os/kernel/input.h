#pragma once

#include <stdint.h>

// IOCSコール関数
int _iocs_b_keyinp(void);
int _iocs_b_keysns(void);

// キーコード変換関数
int x68k_keycode_to_ascii(int keycode);