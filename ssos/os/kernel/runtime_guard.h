#ifndef SS_RUNTIME_GUARD_H
#define SS_RUNTIME_GUARD_H
#include <stdint.h>

/* Finite polling guard. The spin ceiling remains effective if both IRQ clocks
 * stop. Tick timeout is not a wall-clock guarantee when IRQs are masked. */
#define SS_VSYNC_SPIN_LIMIT 5000000UL
#define SS_VSYNC_TICK_LIMIT 100UL

typedef struct {
    uint32_t vsync, tick, spins;
    uint8_t recovered;
} SSWaitGuard;
void ss_wait_guard_init(SSWaitGuard* guard, uint32_t vsync, uint32_t tick);
/* 0 = wait, 1 = ready, -1 = recover once, -2 = stop after failed recovery. */
int ss_wait_guard_poll(SSWaitGuard* guard, uint32_t vsync, uint32_t tick);

/* Cleanup never returns ownership of display/process memory with DMA active.
 * The caller holds IRQ mask 7; vector callbacks preserve that mask.
 * The final display callback may enable the already-restored host IRQs. */
#define SS_CLEAN_INTERRUPTS 1u
#define SS_CLEAN_VECTORS 2u
#define SS_CLEAN_TRAP 4u
#define SS_CLEAN_DISPLAY 8u
typedef struct { uint8_t active, started; } SSCleanupState;
typedef struct {
    int (*dma_stopped)(void* ctx);
    void (*restore)(void* ctx, uint8_t stage);
    void* ctx;
} SSCleanupOps;
/* 0 = restored, -1 = DMA unconfirmed, -2 = cleanup reentry. */
int ss_cleanup_run(volatile SSCleanupState* state, const SSCleanupOps* ops);
#endif
