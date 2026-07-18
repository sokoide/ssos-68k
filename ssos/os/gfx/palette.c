#include "palette.h"
#include "gfx.h"

#include <x68k/iocs.h>

#define PAL_RGB(r, g, b) \
    (uint16_t)((((g) & 0x1F) << 11) | (((r) & 0x1F) << 6) | \
               (((b) & 0x1F) << 1) | 1)

uint16_t ss_palette_index(SSPalette color) {
    if (ss_current_mode->color_count == 256) {
        static const uint16_t color_256[] = {
            [SS_PALETTE_BLACK] = 215,
            [SS_PALETTE_WHITE] = 0,
            [SS_PALETTE_LIGHT_GRAY] = 247,
            [SS_PALETTE_MEDIUM_GRAY] = 250,
            [SS_PALETTE_DARK_GRAY] = 252,
        };
        return color_256[color];
    }

    static const uint16_t color_16[] = {
        [SS_PALETTE_BLACK] = 0,
        [SS_PALETTE_WHITE] = 7,
        [SS_PALETTE_LIGHT_GRAY] = 8,
        [SS_PALETTE_MEDIUM_GRAY] = 15,
        [SS_PALETTE_DARK_GRAY] = 15,
    };
    return color_16[color];
}

void ss_palette_program_default(void) {
    if (ss_current_mode->color_count == 256) {
        /* 256-color palette (6x6x6 cube + 40 system colors). */
        int i, r, g, b;
        int idx = 0;
        static const uint8_t cube_levels[] = {31, 25, 18, 12, 6, 0};
        static const uint8_t system_levels[] = {29, 27, 23, 21, 17,
                                                 15, 10, 8, 4, 2};

        for (r = 0; r < 6; r++)
            for (g = 0; g < 6; g++)
                for (b = 0; b < 6; b++)
                    _iocs_gpalet(idx++, PAL_RGB(cube_levels[r],
                                                cube_levels[g],
                                                cube_levels[b]));

        for (i = 0; i < 10; i++)
            _iocs_gpalet(idx++, PAL_RGB(system_levels[i], 0, 0));
        for (i = 0; i < 10; i++)
            _iocs_gpalet(idx++, PAL_RGB(0, system_levels[i], 0));
        for (i = 0; i < 10; i++)
            _iocs_gpalet(idx++, PAL_RGB(0, 0, system_levels[i]));
        for (i = 0; i < 10; i++)
            _iocs_gpalet(idx++, PAL_RGB(system_levels[i], system_levels[i],
                                        system_levels[i]));
    } else if (ss_current_mode->color_count == 16) {
        static const uint16_t palette_16[16] = {
            PAL_RGB(0, 0, 0),       /* 0: Black */
            PAL_RGB(0, 0, 31),      /* 1: Blue */
            PAL_RGB(0, 31, 0),      /* 2: Green */
            PAL_RGB(0, 31, 31),     /* 3: Cyan */
            PAL_RGB(31, 0, 0),      /* 4: Red */
            PAL_RGB(31, 0, 31),     /* 5: Magenta */
            PAL_RGB(31, 31, 0),     /* 6: Yellow */
            PAL_RGB(31, 31, 31),    /* 7: White */
            PAL_RGB(15, 15, 15),    /* 8: Dark Gray */
            PAL_RGB(15, 15, 31),    /* 9: Light Blue */
            PAL_RGB(15, 31, 15),    /* 10: Light Green */
            PAL_RGB(15, 31, 31),    /* 11: Light Cyan */
            PAL_RGB(31, 15, 15),    /* 12: Light Red */
            PAL_RGB(31, 15, 31),    /* 13: Light Magenta */
            PAL_RGB(31, 31, 15),    /* 14: Light Yellow */
            PAL_RGB(0, 0, 0),       /* 15: Black (duplicate) */
        };
        for (int i = 0; i < 16; i++)
            _iocs_gpalet(i, palette_16[i]);
    }
}
