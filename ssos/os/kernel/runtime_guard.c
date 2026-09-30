#include "runtime_guard.h"

void ss_wait_guard_init(SSWaitGuard* g, uint32_t vsync, uint32_t tick) {
    g->vsync = vsync;
    g->tick = tick;
    g->spins = 0;
    g->recovered = 0;
}

int ss_wait_guard_poll(SSWaitGuard* g, uint32_t vsync, uint32_t tick) {
    if (vsync != g->vsync) return 1;
    if (++g->spins < SS_VSYNC_SPIN_LIMIT &&
        tick - g->tick < SS_VSYNC_TICK_LIMIT) return 0;
    if (g->recovered) return -2;
    g->recovered = 1;
    g->spins = 0;
    g->tick = tick;
    return -1;
}

int ss_cleanup_run(volatile SSCleanupState* s, const SSCleanupOps* ops) {
    if (s->started) return -2;
    s->started = 1;
    if (!ops->dma_stopped(ops->ctx)) return -1;
    static const uint8_t stages[] = {
        SS_CLEAN_INTERRUPTS, SS_CLEAN_VECTORS, SS_CLEAN_TRAP, SS_CLEAN_DISPLAY
    };
    for (unsigned i = 0; i < sizeof(stages); i++) {
        if (s->active & stages[i]) {
            s->active &= (uint8_t)~stages[i];
            ops->restore(ops->ctx, stages[i]);
        }
    }
    return 0;
}
