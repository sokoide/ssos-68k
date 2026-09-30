#include "scene_replay.h"
#include "../win/win.h"
#include "../gfx/gfx.h"

int ss_scene_replay_input(void* ctx, uint32_t frame, int* mx, int* my,
                          int* buttons, int* key) {
    (void)ctx;
    static const int inputs[11][3] = {
        {50,20,512}, {400,300,512}, {400,300,0},
        {200,65,512}, {300,150,512}, {300,150,0},
        {10,400,0}, {290,300,512}, {98,125,512}, {98,125,0}, {10,400,0}
    };
    if (frame > SS_SCENE_REPLAY_FRAMES) return 1;
    if (frame <= 11) {
        *mx = inputs[frame - 1][0]; *my = inputs[frame - 1][1];
        *buttons = inputs[frame - 1][2];
    } else if (frame <= 515) {
        *mx = 400; *my = 150; *buttons = (frame & 1) ? 0 : 512;
    } else {
        if (frame == 516) {
            ss_win_move(3, ss_current_mode->display_w - 22, 120);
            ss_win_render_all();
            ss_gfx_xor_rect(400, 150, 6, 6);
        }
        *mx = frame == 516 ? 1000 : 9; *my = 400; *buttons = 0;
    }
    *key = -1;
    return 0;
}
