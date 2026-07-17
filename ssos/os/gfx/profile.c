#include "profile.h"

#if SS_PROFILE_GFX
#include <string.h>

SSGfxProfile ss_gfx_profile;

void ss_gfx_profile_reset(void) {
    memset(&ss_gfx_profile, 0, sizeof(ss_gfx_profile));
}

void ss_gfx_profile_snapshot(SSGfxProfile* out) {
    if (out) {
        *out = ss_gfx_profile;
    }
}
#else
void ss_gfx_profile_reset(void) {
}

void ss_gfx_profile_snapshot(SSGfxProfile* out) {
    (void)out;
}
#endif
