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

#define SS_SCENE_WINDOW_COUNT 3

/* The normal UI and standalone benchmark use the same model layout.  Their
 * renderers deliberately remain separate: the benchmark measures primitive
 * compositor paths while the UI uses its content-aware callback. */
typedef struct {
    int x, y, w, h;
    uint16_t z;
    const char *title;
} SSSceneWindowSpec;

extern const SSSceneWindowSpec ss_scene_default_windows[SS_SCENE_WINDOW_COUNT];

void ss_scene_run(const SSSceneHooks *hooks, SSSceneStats *stats);
int ss_scene_last_key(void);

#endif /* SS_APP_SCENE_H */
