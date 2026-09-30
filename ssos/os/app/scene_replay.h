#ifndef SS_SCENE_REPLAY_H
#define SS_SCENE_REPLAY_H
#include "scene.h"
#define SS_SCENE_REPLAY_FRAMES 517u
/* Same input sequence for host RAM tests and target. No IOCS input sampling.
 * Edge placement is an explicit test injection beyond UI drag clamping. */
int ss_scene_replay_input(void* ctx, uint32_t frame, int* mx, int* my,
                          int* buttons, int* key);
#endif
