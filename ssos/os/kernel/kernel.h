#pragma once

#include <stdint.h>

extern volatile uint8_t* mfp;

void ss_wait_for_vsync();

int ss_handle_keys();

struct KeyBuffer {
    int data[32];
    int idxr;
    int idxw;
    int len;
};

extern struct KeyBuffer kb;
