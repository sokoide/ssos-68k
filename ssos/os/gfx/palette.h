#ifndef SS_PALETTE_H
#define SS_PALETTE_H

#include <stdint.h>

/* Logical UI colors.  The returned hardware index depends on the mode. */
typedef enum {
    SS_PALETTE_BLACK = 0,
    SS_PALETTE_WHITE,
    SS_PALETTE_LIGHT_GRAY,
    SS_PALETTE_MEDIUM_GRAY,
    SS_PALETTE_DARK_GRAY,
} SSPalette;

uint16_t ss_palette_index(SSPalette color);

/* Program the standard 16/256-color palettes for ss_current_mode. */
void ss_palette_program_default(void);

#endif /* SS_PALETTE_H */
