#include "ssos_test.h"
#include "runtime_guard.h"
#include <string.h>

static int stopped, restores, reentry;
static SSCleanupState* nested_state;
static SSCleanupOps ops;
static int dma_check(void* ctx) { (void)ctx; return stopped; }
static void restore(void* ctx, uint8_t stage) {
    (void)ctx;
    restores = restores * 16 + stage;
    if (nested_state) {
        reentry = ss_cleanup_run(nested_state, &ops);
        nested_state = NULL;
    }
}
TEST(cleanup_partial_order_and_reentry) {
    ops = (SSCleanupOps){dma_check, restore, NULL};
    stopped = 1;
    for (unsigned active = 0; active < 16; active++) {
        SSCleanupState s = {active, 0};
        restores = 0;
        nested_state = &s;
        ASSERT_EQ(ss_cleanup_run(&s, &ops), 0);
        ASSERT_EQ(s.active, 0);
        int expected = 0;
        for (unsigned bit = 1; bit <= 8; bit <<= 1)
            if (active & bit) expected = expected * 16 + bit;
        ASSERT_EQ(restores, expected);
        if (active) ASSERT_EQ(reentry, -2);
        ASSERT_EQ(ss_cleanup_run(&s, &ops), -2);
        nested_state = NULL;
    }
}
TEST(cleanup_dma_unconfirmed_preserves_all_resources) {
    ops = (SSCleanupOps){dma_check, restore, NULL};
    stopped = 0; restores = 0;
    SSCleanupState s = {15, 0};
    ASSERT_EQ(ss_cleanup_run(&s, &ops), -1);
    ASSERT_EQ(s.active, 15);
    ASSERT_EQ(restores, 0);
}
TEST(wait_guard_stopped_clocks_are_finite) {
    SSWaitGuard g;
    ss_wait_guard_init(&g, 0xffffffffu, 0xffffffffu);
    g.spins = SS_VSYNC_SPIN_LIMIT - 1;
    ASSERT_EQ(ss_wait_guard_poll(&g, 0xffffffffu, 0xffffffffu), -1);
    g.spins = SS_VSYNC_SPIN_LIMIT - 1;
    ASSERT_EQ(ss_wait_guard_poll(&g, 0xffffffffu, 0xffffffffu), -2);
    ASSERT_EQ(ss_wait_guard_poll(&g, 0, 0), 1);
}
TEST(wait_guard_tick_timeout_wrap_and_recovery) {
    SSWaitGuard g;
    ss_wait_guard_init(&g, 9, 0xfffffffeu);
    ASSERT_EQ(ss_wait_guard_poll(&g, 9, 1), 0);
    ASSERT_EQ(ss_wait_guard_poll(&g, 9, 98), -1);
    ASSERT_EQ(ss_wait_guard_poll(&g, 10, 99), 1);
    ss_wait_guard_init(&g, 9, 0);
    ASSERT_EQ(ss_wait_guard_poll(&g, 9, 100), -1);
    ASSERT_EQ(ss_wait_guard_poll(&g, 9, 200), -2);
}
void run_runtime_guard_tests(void) {
    RUN_TEST(cleanup_partial_order_and_reentry);
    RUN_TEST(cleanup_dma_unconfirmed_preserves_all_resources);
    RUN_TEST(wait_guard_stopped_clocks_are_finite);
    RUN_TEST(wait_guard_tick_timeout_wrap_and_recovery);
}
