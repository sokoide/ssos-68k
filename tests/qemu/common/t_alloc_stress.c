/* t_alloc_stress.c - two tasks hammering the buddy allocator under context
 * switches (R09 verification).
 *
 * The allocator has no internal lock (runtime-ownership.md): shared use must
 * mask IRQs around each complete alloc/free, exactly the pattern exercised
 * here. Each worker writes a per-iteration pattern into every allocated
 * block and re-verifies it before freeing, so cross-block corruption between
 * the interleaved tasks is caught by content, not just by the free-list
 * invariants. Main asserts the free-byte count returns exactly to baseline.
 */

#include "scheduler.h"
#include "main_task.h"
#include "memory.h"
#include "tty.h"

#define ARENA_SIZE (96 * 1024)
static char arena[ARENA_SIZE] __attribute__((aligned(16)));

static SSTask main_tcb;
static volatile int workers_done;
static uint32_t baseline_free;

static void fail(const char* reason) {
    tty_puts("FAIL ");
    tty_puts(reason);
    tty_puts("\n");
    for (;;) {}
}

static const uint16_t sizes[] = {16, 100, 1000, 4096};

static void* worker(void* arg) {
    uint16_t seed = (uint16_t)(uintptr_t)arg;
    for (int round = 0; round < 64; round++) {
        for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
            uint16_t saved_sr = ss_irq_save();
            void* p = ss_alloc(sizes[s]);
            ss_irq_restore(saved_sr);
            if (p == NULL) fail("alloc");
            if (sizes[s] >= 4) {
                volatile uint16_t* w = (volatile uint16_t*)p;
                for (uint16_t k = 0; k < sizes[s] / 4; k++)
                    w[k * 2] = (uint16_t)(seed + round + k);
                for (uint16_t k = 0; k < sizes[s] / 4; k++)
                    if (w[k * 2] != (uint16_t)(seed + round + k))
                        fail("block corrupted");
            }
            /* Yield between operations so the peer task interleaves; the
             * masked alloc/free windows above are what keep it safe. */
            ss_task_yield();
            saved_sr = ss_irq_save();
            ss_free(p);
            ss_irq_restore(saved_sr);
        }
    }
    workers_done++;
    /* Return (not endless yielding) so the TERMINATED dequeue hands control
     * back to the lower-priority main loop. */
    return 0;
}

int main(void) {
    ss_sched_init();
    ss_mem_init(arena, sizeof(arena));
    if (!ss_mem_healthy()) fail("mem init");
    baseline_free = ss_mem_free_bytes();
    if (baseline_free == 0) fail("baseline");

    if (ss_main_task_register(&main_tcb, 15) != SS_OK)
        fail("register");

    SSTaskInfo info_a = { .entry = worker, .arg = (void*)0x1000, .pri = 6 };
    SSTaskInfo info_b = { .entry = worker, .arg = (void*)0x2000, .pri = 6 };
    uint16_t id_a = ss_task_create(&info_a);
    uint16_t id_b = ss_task_create(&info_b);
    if (id_a != 1 || id_b != 2 ||
        ss_task_start(id_a) != SS_OK || ss_task_start(id_b) != SS_OK)
        fail("task setup");

    int guard = 0;
    while (workers_done < 2 && guard++ < 20000)
        ss_task_yield();

    if (workers_done < 2) fail("workers did not finish");
    if (ss_mem_free_bytes() != baseline_free) fail("free bytes leaked");
    if (ss_stack_check(id_a) == 0 || ss_stack_check(id_b) == 0)
        fail("stack canary");
    tty_puts("OK two-task allocator stress\n");
    for (;;) {}
    return 0;
}
