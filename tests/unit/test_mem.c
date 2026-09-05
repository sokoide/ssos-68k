/* test_mem.c - tests for the buddy allocator and slab cache.
 *
 * Learning objective: buddy.c/slab.c are pure software (no HW), so they run
 * unmodified on the host. We hand ss_mem_init() a plain static buffer and
 * verify allocation, coalescing, alignment, and slab object pools.
 *
 * Note: sizeof(SSBuddyBlock) differs between the m68k target (4-byte pointer)
 * and the 64-bit host (8-byte pointer), so tests derive sizes from the struct
 * rather than hard-coding byte counts. */

#include "memory.h"
#include "ssos_test.h"

#include <stdint.h>
#include <string.h>

/* Test arena. 256KB is enough to hold the order map plus several max-order
 * (64KB) blocks. Aligned so the base address itself is sane. */
static uint8_t arena[256 * 1024] __attribute__((aligned(16)));
static uint8_t tiny_arena[16] __attribute__((aligned(16)));
static uint8_t slab_overflow_arena[(UINT16_MAX + 1u) * sizeof(SSSlabObj)];

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

TEST(mem_init_preserves_all_usable_blocks) {
    const uint32_t min_block = 1u << SS_BUDDY_MIN_ORDER;
    const uint32_t sizes[] = {32, 48, 560, 4096, 65536, sizeof(arena) - 1,
                              sizeof(arena)};
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        uint32_t size = sizes[i] & ~(min_block - 1);
        uint32_t map_size = size / min_block;
        uint32_t reserved = (map_size + min_block - 1) & ~(min_block - 1);
        ss_mem_init(arena, sizes[i]);
        ASSERT_EQ(ss_mem_free_bytes(), size - reserved);
    }
}

TEST(alloc_exhausts_tail_without_overlap_and_recovers) {
    /* 4096 bytes reserve 256 map bytes, leaving 120 blocks of 32 bytes. */
    enum { arena_size = 4096, block_size = 32, block_count = 120 };
    void* blocks[block_count];
    uint32_t payload = block_size - sizeof(SSBuddyBlock);
    ss_mem_init(arena, arena_size);
    for (int i = 0; i < block_count; i++) {
        blocks[i] = ss_alloc(payload);
        ASSERT_NOT_NULL(blocks[i]);
        ASSERT_TRUE((uint8_t*)blocks[i] >= arena + 256);
        ASSERT_TRUE((uint8_t*)blocks[i] + payload <= arena + arena_size);
        memset(blocks[i], i + 1, payload);
    }
    ASSERT_NULL(ss_alloc(payload));
    ASSERT_EQ(ss_mem_free_bytes(), 0);
    for (int i = 0; i < block_count; i++) {
        for (uint32_t j = 0; j < payload; j++) {
            ASSERT_EQ(((uint8_t*)blocks[i])[j], i + 1);
        }
    }
    /* Interleave frees to exercise merging across multiple initial orders. */
    for (int parity = 0; parity < 2; parity++) {
        for (int i = parity; i < block_count; i += 2) ss_free(blocks[i]);
    }
    ASSERT_EQ(ss_mem_free_bytes(), 3840);
    const uint32_t orders[] = {2048, 1024, 512, 256};
    for (unsigned i = 0; i < sizeof(orders) / sizeof(orders[0]); i++) {
        blocks[i] = ss_alloc(orders[i] - sizeof(SSBuddyBlock));
        ASSERT_NOT_NULL(blocks[i]);
    }
    ASSERT_EQ(ss_mem_free_bytes(), 0);
    for (int i = 3; i >= 0; i--) ss_free(blocks[i]);
    ASSERT_EQ(ss_mem_free_bytes(), 3840);
}

TEST(mem_init_null_or_too_small_is_empty) {
    ss_mem_init(arena, sizeof(arena));
    ss_mem_init(NULL, sizeof(arena));
    ASSERT_EQ(ss_mem_total(), 0);
    ASSERT_EQ(ss_mem_free_bytes(), 0);
    ASSERT_NULL(ss_alloc(1));

    ss_mem_init(tiny_arena, sizeof(tiny_arena));
    ASSERT_EQ(ss_mem_total(), 0);
    ASSERT_EQ(ss_mem_free_bytes(), 0);
    ASSERT_NULL(ss_alloc(1));

    ss_mem_init(arena + 1, sizeof(arena) - 1);
    ASSERT_EQ(ss_mem_total(), 0);
    ASSERT_EQ(ss_mem_free_bytes(), 0);
    ASSERT_NULL(ss_alloc(1));
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

TEST(alloc_rejects_header_overflow_and_max_block_excess) {
    ss_mem_init(arena, sizeof(arena));
    ASSERT_NULL(ss_alloc(UINT32_MAX));
    ASSERT_NULL(ss_alloc((1u << SS_BUDDY_MAX_ORDER) -
                         (uint32_t)sizeof(SSBuddyBlock) + 1));
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

TEST(split_block_coalesces_back_to_max_order) {
    /* 69920 bytes reserve 4384 map bytes, leaving exactly one 64KB block. */
    ss_mem_init(arena, 69920);
    uint32_t max_payload = (1u << SS_BUDDY_MAX_ORDER) - sizeof(SSBuddyBlock);
    void* small = ss_alloc(1);
    void* large[SS_BUDDY_ORDERS];
    int large_count = 0;
    ASSERT_NOT_NULL(small);

    while (large_count < SS_BUDDY_ORDERS) {
        void* block = ss_alloc(max_payload);
        if (block == NULL)
            break;
        large[large_count++] = block;
    }
    ASSERT_NULL(ss_alloc(max_payload));

    ss_free(small);
    void* merged = ss_alloc(max_payload);
    ASSERT_NOT_NULL(merged);
    ss_free(merged);
    for (int i = 0; i < large_count; i++) ss_free(large[i]);
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

TEST(alloc_aligned_rejects_overflow_and_oversized_alignment) {
    ss_mem_init(arena, sizeof(arena));
    ASSERT_NULL(ss_alloc_aligned(UINT32_MAX, 16));
    ASSERT_NULL(ss_alloc_aligned(1, 1u << (SS_BUDDY_MAX_ORDER + 1)));
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

TEST(slab_init_invalid_input_is_empty) {
    SSSlabCache cache;

    ss_slab_init(&cache, 0, arena, sizeof(arena));
    ASSERT_EQ(cache.count, 0);
    ASSERT_NULL(ss_slab_alloc(&cache));

    ss_slab_init(&cache, (uint16_t)(sizeof(SSSlabObj) - 1), arena,
                 sizeof(arena));
    ASSERT_EQ(cache.count, 0);
    ASSERT_NULL(ss_slab_alloc(&cache));

    ss_slab_init(&cache, (uint16_t)(sizeof(SSSlabObj) + 1), arena,
                 sizeof(arena));
    ASSERT_EQ(cache.count, 0);
    ASSERT_NULL(ss_slab_alloc(&cache));

    ss_slab_init(&cache, (uint16_t)sizeof(SSSlabObj), arena + 1,
                 sizeof(arena) - 1);
    ASSERT_EQ(cache.count, 0);
    ASSERT_NULL(ss_slab_alloc(&cache));

    ss_slab_init(&cache, (uint16_t)sizeof(SSSlabObj), NULL, sizeof(arena));
    ASSERT_EQ(cache.count, 0);
    ASSERT_NULL(ss_slab_alloc(&cache));

    ss_slab_init(&cache, (uint16_t)sizeof(SSSlabObj), arena,
                 (uint32_t)sizeof(SSSlabObj) - 1);
    ASSERT_EQ(cache.count, 0);
    ASSERT_NULL(ss_slab_alloc(&cache));
}

TEST(slab_init_rejects_count_overflow_and_null_cache_ops) {
    SSSlabCache cache;

    ss_slab_init(&cache, (uint16_t)sizeof(SSSlabObj), slab_overflow_arena,
                 sizeof(slab_overflow_arena));
    ASSERT_EQ(cache.count, 0);
    ASSERT_NULL(ss_slab_alloc(&cache));
    ASSERT_NULL(ss_slab_alloc(NULL));
    ss_slab_init(NULL, (uint16_t)sizeof(SSSlabObj), arena, sizeof(arena));
    ss_slab_free(NULL, arena);
}

void run_mem_tests(void) {
    RUN_TEST(mem_init_reports_total);
    RUN_TEST(mem_init_has_free_space);
    RUN_TEST(mem_init_preserves_all_usable_blocks);
    RUN_TEST(alloc_exhausts_tail_without_overlap_and_recovers);
    RUN_TEST(mem_init_null_or_too_small_is_empty);
    RUN_TEST(alloc_returns_valid_pointer);
    RUN_TEST(alloc_free_restores_free_bytes);
    RUN_TEST(alloc_zero_returns_null);
    RUN_TEST(alloc_huge_returns_null);
    RUN_TEST(alloc_rejects_header_overflow_and_max_block_excess);
    RUN_TEST(two_allocs_reverse_free_coalesces);
    RUN_TEST(split_block_coalesces_back_to_max_order);
    RUN_TEST(repeated_alloc_free_no_leak);
    RUN_TEST(alloc_aligned_is_4k_aligned);
    RUN_TEST(alloc_aligned_roundtrip_restores);
    RUN_TEST(alloc_aligned_rejects_overflow_and_oversized_alignment);
    RUN_TEST(slab_init_counts);
    RUN_TEST(slab_alloc_decrements_free);
    RUN_TEST(slab_alloc_until_exhausted);
    RUN_TEST(slab_free_restores_and_is_null_safe);
    RUN_TEST(slab_init_invalid_input_is_empty);
    RUN_TEST(slab_init_rejects_count_overflow_and_null_cache_ops);
}
