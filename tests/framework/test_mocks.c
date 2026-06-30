/* test_mocks.c - HW/asm stubs for native testing.
 *
 * The kernel (scheduler) and window sources compile unchanged on the host
 * once their HW/asm dependencies are stubbed here. Each stub has a comment
 * explaining what the real implementation does and why the stub is safe for
 * the logic under test. See tests/README.md for the test-scope limitations.
 *
 * Stubbed dependencies:
 *   - Interrupt enable/disable  (real: move.w #imm,%sr)  -> no-op
 *   - ss_task_yield             (real: asm context switch) -> call ss_do_context_switch()
 *   - ss_tick_counter et al.    (real: bumped by Timer D ISR) -> host-controlled vars
 *   - ss_task_stack_base        (real: app-provided)        -> static arena
 *   - ss_wakeups_needed (coop.) (real: set by ISR)          -> host-controlled var
 *   - ss_gfx_rect/fill_stipple  (real: VRAM/DMA writes)     -> call counters
 *   - ss_current_mode           (real: set by gfx init)     -> dummy mode
 */

#include "ssos_test.h"
#include "kernel.h"
#include "scheduler.h"
#include "gfx.h"

#include <stdint.h>

/* ---- 1. Interrupt enable/disable -------------------------------------- */
/* Real: move.w #0x2700/%sr (disable) / #0x2000/%sr (enable). The scheduler
 * uses these only to guard queue critical sections; no-op is correct because
 * the test is single-threaded. */
void ss_disable_interrupts(void) { }
void ss_enable_interrupts(void)  { }

/* ---- 2. Tick/vsync counters (defined in interrupts.s on real HW) ------ */
volatile uint32_t ss_tick_counter      = 0;
volatile uint32_t ss_vsync_counter     = 0;
volatile uint32_t ss_vdisp_fire_count  = 0;
volatile uint32_t ss_timerd_fire_count = 0;

/* ---- 3. ss_task_yield (asm on real HW) -------------------------------- */
/* Real: builds a yield frame and switches stacks via ss_do_context_switch +
 * resume_task. The stub drives only the C queue logic (round-robin move +
 * pick) so sleep/wakeup state transitions can be observed. It does NOT
 * actually swap register state — tests stay single-threaded. */
void ss_task_yield(void) {
    ss_do_context_switch();
}

/* ---- 4. Task stack arena (real: provided by app/main.c) --------------- */
static uint8_t test_stack_mem[SS_MAX_TASKS * SS_TASK_STACK]
    __attribute__((aligned(4)));
uint8_t* ss_task_stack_base = test_stack_mem;

/* ---- 5. cooperative-only wakeup flag ---------------------------------- */
#ifdef SS_BUILD_COOPERATIVE
volatile uint8_t ss_wakeups_needed = 0;
#endif

/* ---- 6. Graphics stubs (real: VRAM/DMA in vram.c) --------------------- */
/* Count calls so render_all tests can assert the window system actually
 * painted (and skipped occluded / hidden windows). */
int gfx_rect_calls         = 0;
int gfx_fill_stipple_calls = 0;

void ss_gfx_rect(int x, int y, int w, int h, uint16_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
    gfx_rect_calls++;
}

void ss_gfx_fill_stipple(int x, int y, int w, int h, uint16_t c1, uint16_t c2) {
    (void)x; (void)y; (void)w; (void)h; (void)c1; (void)c2;
    gfx_fill_stipple_calls++;
}

/* Dummy mode: only display_w/display_h are read by window.c (render_all
 * background stipple). Keep it modest so stipple call count stays small. */
static const SSGfxMode test_mode = {
    .crtmod = 16, .screen_w = 512, .screen_h = 512,
    .display_w = 128, .display_h = 128,
    .color_count = 16, .page_count = 1,
    .bytes_per_line = 128, .page_size = 128 * 128,
    .page0 = NULL, .page1 = NULL,
};
const SSGfxMode* ss_current_mode = &test_mode;

/* ---- 7. Reset --------------------------------------------------------- */
void reset_test_state(void) {
    ss_tick_counter = 0;
    ss_vsync_counter = 0;
    ss_vdisp_fire_count = 0;
    ss_timerd_fire_count = 0;
#ifdef SS_BUILD_COOPERATIVE
    ss_wakeups_needed = 0;
#endif
    gfx_rect_calls = 0;
    gfx_fill_stipple_calls = 0;
    /* scheduler/window static state is reset by ss_sched_init()/ss_win_init()
     * at the start of each test that touches them. */
}

/* Deprecated alias kept for compatibility with the original framework. */
void reset_mocks(void) {
    reset_test_state();
}
