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
 *   - graphics MMIO             (real: VRAM/CRTC/DMAC)      -> RAM seam in vram.c
 */

#include "ssos_test.h"
#include "kernel.h"
#include "scheduler.h"
#include "gfx.h"
#include "palette.h"

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

/* Palette programming is hardware-only.  Window tests require only the
 * logical 16-color indices used by the shared compositor. */
uint16_t ss_palette_index(SSPalette color) {
    static const uint16_t indices[] = {0, 7, 8, 15};
    return indices[color];
}

/* ---- 7. Reset --------------------------------------------------------- */
void reset_test_state(void) {
    ss_tick_counter = 0;
    ss_vsync_counter = 0;
    ss_vdisp_fire_count = 0;
    ss_timerd_fire_count = 0;
#ifdef SS_BUILD_COOPERATIVE
    ss_wakeups_needed = 0;
#endif
    /* scheduler/window static state is reset by ss_sched_init()/ss_win_init()
     * at the start of each test that touches them. */
}

/* Deprecated alias kept for compatibility with the original framework. */
void reset_mocks(void) {
    reset_test_state();
}
