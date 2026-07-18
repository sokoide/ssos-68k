#ifndef SS_APP_SCENE_H
#define SS_APP_SCENE_H

#include <stdint.h>

/* Host-independent UI scene.  The host owns IOCS/MFP setup and supplies
 * optional wait/stop hooks; rendering, input sampling, drag and content
 * updates live in the shared implementation. */
typedef struct {
    int (*wait_vsync)(void *ctx); /* 0 = frame ready, nonzero = stop */
    int (*should_stop)(void *ctx);
    void *ctx;
} SSSceneHooks;

typedef struct {
    uint32_t frames;
    uint32_t vsyncs;
} SSSceneStats;

void ss_scene_run(const SSSceneHooks *hooks, SSSceneStats *stats);
int ss_scene_last_key(void);

#endif /* SS_APP_SCENE_H */
