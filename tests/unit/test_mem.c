/* test_mem.c - tests for the buddy allocator and slab cache.
 *
 * Learning objective: buddy.c/slab.c are pure software (no HW), so they run
 * unmodified on the host. We hand ss_mem_init() a plain static buffer and
 * verify allocation, coalescing, alignment, and slab object pools.
 *
 * Note: sizeof(SSBuddyBlock) differs between the m68k target (4-byte pointer)
 * and the 64-bit host (8-byte pointer), so tests derive sizes from the struct
 * rather than hard-coding byte counts. */

#include "ssos_test.h"
#include "memory.h"

#include <stdint.h>
#include <string.h>

/* Test arena. 256KB is enough to hold the order map plus several max-order
 * (64KB) blocks. Aligned so the base address itself is sane. */
static uint8_t arena[256 * 1024] __attribute__((aligned(16)));

/* ---- buddy: init ---- */

TEST(mem_init_reports_total) {
    ss_mem_init(arena, sizeof(arena));
    ASSERT_EQ(ss_mem_total(), (uint32_t)sizeof(arena));
}

TEST(mem_init_has_free_space) {
    ss_mem_init(arena, sizeof(arena));
    /* After reserving the order map, most of the arena must remain free. */
    ASSERT_TRUE(ss_mem_free_bytes() > (uint32_t)(128 * 1024));
}

/* ---- buddy: basic alloc/free ---- */

TEST(alloc_returns_valid_pointer) {
    ss_mem_init(arena, sizeof(arena));
    void* p = ss_alloc(100);
    ASSERT_NOT_NULL(p);
    /* Pointer must lie inside the arena. */
    ASSERT_TRUE((uint8_t*)p >= arena && (uint8_t*)p < arena + sizeof(arena));
}

TEST(alloc_free_restores_free_bytes) {
    ss_mem_init(arena, sizeof(arena));
    uint32_t before = ss_mem_free_bytes();
    void* p = ss_alloc(100);
    ASSERT_NOT_NULL(p);
    ASSERT_TRUE(ss_mem_free_bytes() < before);
    ss_free(p);
    /* A lone block freed alone still coalesces back to its original order. */
    ASSERT_EQ(ss_mem_free_bytes(), before);
}

TEST(alloc_zero_returns_null) {
    ss_mem_init(arena, sizeof(arena));
    ASSERT_NULL(ss_alloc(0));
}

TEST(alloc_huge_returns_null) {
    ss_mem_init(arena, sizeof(arena));
    /* Far beyond the max order (64KB). */
    ASSERT_NULL(ss_alloc(200000));
}

/* ---- buddy: coalescing ---- */

TEST(two_allocs_reverse_free_coalesces) {
    ss_mem_init(arena, sizeof(arena));
    uint32_t before = ss_mem_free_bytes();
    void* a = ss_alloc(100);
    void* b = ss_alloc(100);
    ASSERT_NOT_NULL(a);
    ASSERT_NOT_NULL(b);
    ASSERT_NEQ(a, b);
    /* Freeing in reverse order lets buddies merge back up the order tree. */
    ss_free(b);
    ss_free(a);
    ASSERT_EQ(ss_mem_free_bytes(), before);
}

TEST(repeated_alloc_free_no_leak) {
    ss_mem_init(arena, sizeof(arena));
    uint32_t before = ss_mem_free_bytes();
    for (int i = 0; i < 50; i++) {
        void* p = ss_alloc(256);
        ASSERT_NOT_NULL(p);
        ss_free(p);
    }
    ASSERT_EQ(ss_mem_free_bytes(), before);
}

/* ---- buddy: aligned allocation ---- */

TEST(alloc_aligned_is_4k_aligned) {
    ss_mem_init(arena, sizeof(arena));
    void* p = ss_alloc_aligned(100, 4096);
    ASSERT_NOT_NULL(p);
    ASSERT_ALIGNED_4K(p);
    ss_free_aligned(p);
}

TEST(alloc_aligned_roundtrip_restores) {
    ss_mem_init(arena, sizeof(arena));
    uint32_t before = ss_mem_free_bytes();
    void* p = ss_alloc_aligned(1000, 4096);
    ASSERT_NOT_NULL(p);
    ASSERT_ALIGNED_4K(p);
    ss_free_aligned(p);
    ASSERT_EQ(ss_mem_free_bytes(), before);
}

/* ---- slab ---- */

TEST(slab_init_counts) {
    SSSlabCache cache;
    /* 8-byte objects in a 1024-byte region -> 128 objects. */
    ss_slab_init(&cache, 8, arena, 1024);
    ASSERT_EQ(cache.count, 128);
    ASSERT_EQ(cache.free_count, 128);
}

TEST(slab_alloc_decrements_free) {
    SSSlabCache cache;
    ss_slab_init(&cache, 8, arena, 1024);
    void* o = ss_slab_alloc(&cache);
    ASSERT_NOT_NULL(o);
    ASSERT_EQ(cache.free_count, 127);
}

TEST(slab_alloc_until_exhausted) {
    SSSlabCache cache;
    ss_slab_init(&cache, 8, arena, 1024);
    for (int i = 0; i < 128; i++) {
        ASSERT_NOT_NULL(ss_slab_alloc(&cache));
    }
    /* 129th allocation must fail. */
    ASSERT_NULL(ss_slab_alloc(&cache));
    ASSERT_EQ(cache.free_count, 0);
}

TEST(slab_free_restores_and_is_null_safe) {
    SSSlabCache cache;
    ss_slab_init(&cache, 8, arena, 1024);
    void* o = ss_slab_alloc(&cache);
    ss_slab_free(&cache, o);
    ASSERT_EQ(cache.free_count, 128);
    /* Freeing NULL must be a no-op, not a crash. */
    ss_slab_free(&cache, NULL);
    ASSERT_EQ(cache.free_count, 128);
}

void run_mem_tests(void) {
    RUN_TEST(mem_init_reports_total);
    RUN_TEST(mem_init_has_free_space);
    RUN_TEST(alloc_returns_valid_pointer);
    RUN_TEST(alloc_free_restores_free_bytes);
    RUN_TEST(alloc_zero_returns_null);
    RUN_TEST(alloc_huge_returns_null);
    RUN_TEST(two_allocs_reverse_free_coalesces);
    RUN_TEST(repeated_alloc_free_no_leak);
    RUN_TEST(alloc_aligned_is_4k_aligned);
    RUN_TEST(alloc_aligned_roundtrip_restores);
    RUN_TEST(slab_init_counts);
    RUN_TEST(slab_alloc_decrements_free);
    RUN_TEST(slab_alloc_until_exhausted);
    RUN_TEST(slab_free_restores_and_is_null_safe);
}
