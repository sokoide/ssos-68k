#include "../framework/ssos_test.h"

// Include SSOS memory system (LOCAL_MODE enabled in Makefile)
#include "memory.h"
#include "ss_config.h"

// Test helper: Initialize memory system for testing
static void setup_memory_system(void) {
    // Initialize memory manager with test configuration
    ss_mem_init();
}

// Test helper: Reset memory system
static void teardown_memory_system(void) {
    // Reset memory manager state
    ss_mem_mgr.num_free_blocks = 0;

    // Add one large free block for fresh start
    ss_mem_mgr.free_blocks[0].addr = 0x100000;  // Test memory base
    ss_mem_mgr.free_blocks[0].sz = 0x100000;    // 1MB for testing
    ss_mem_mgr.num_free_blocks = 1;
}

// Test basic memory allocation
TEST(memory_alloc_basic) {
    setup_memory_system();
    teardown_memory_system();

    // Test basic allocation
    uint32_t addr1 = ss_mem_alloc(1024);
    ASSERT_NEQ(addr1, 0);

    // Test second allocation
    uint32_t addr2 = ss_mem_alloc(2048);
    ASSERT_NEQ(addr2, 0);
    ASSERT_NEQ(addr1, addr2);

    // Addresses should be sequential (addr2 = addr1 + 1024)
    ASSERT_EQ(addr2, addr1 + 1024);
}

// Test zero-size allocation
TEST(memory_alloc_zero_size) {
    setup_memory_system();
    teardown_memory_system();

    uint32_t addr = ss_mem_alloc(0);
    ASSERT_EQ(addr, 0);  // Should return 0 for zero-size allocation
}

// Test 4K-aligned allocation
TEST(memory_alloc_4k_aligned) {
    setup_memory_system();
    teardown_memory_system();

    // Test various sizes that should be 4K-aligned
    uint32_t addr1 = ss_mem_alloc4k(1);      // Should round up to 4K
    uint32_t addr2 = ss_mem_alloc4k(4095);   // Should round up to 4K
    uint32_t addr3 = ss_mem_alloc4k(4096);   // Already 4K
    uint32_t addr4 = ss_mem_alloc4k(4097);   // Should round up to 8K

    ASSERT_NOT_NULL(addr1);
    ASSERT_NOT_NULL(addr2);
    ASSERT_NOT_NULL(addr3);
    ASSERT_NOT_NULL(addr4);

    // All addresses should be 4K-aligned
    ASSERT_ALIGNED_4K(addr1);
    ASSERT_ALIGNED_4K(addr2);
    ASSERT_ALIGNED_4K(addr3);
    ASSERT_ALIGNED_4K(addr4);

    // Check spacing between allocations
    ASSERT_EQ(addr2, addr1 + 4096);  // First two are 4K each
    ASSERT_EQ(addr3, addr2 + 4096);  // Second two are 4K each
    ASSERT_EQ(addr4, addr3 + 4096);  // Third is 4K, fourth is 8K but starts 4K after
}

// Test out of memory scenario
TEST(memory_alloc_out_of_memory) {
    setup_memory_system();
    teardown_memory_system();

    // Try to allocate more than available (we have 1MB test memory)
    uint32_t addr = ss_mem_alloc(0x200000);  // 2MB request
    ASSERT_EQ(addr, 0);  // Should fail and return 0
}

// Test memory fragmentation scenario
TEST(memory_alloc_fragmentation) {
    setup_memory_system();
    teardown_memory_system();

    // Create fragmentation by allocating several blocks
    uint32_t addr1 = ss_mem_alloc(1024);
    uint32_t addr2 = ss_mem_alloc(2048);
    uint32_t addr3 = ss_mem_alloc(1024);

    ASSERT_NOT_NULL(addr1);
    ASSERT_NOT_NULL(addr2);
    ASSERT_NOT_NULL(addr3);

    // Verify they are sequential as expected with our allocator
    ASSERT_EQ(addr2, addr1 + 1024);
    ASSERT_EQ(addr3, addr2 + 2048);
}

// Test memory manager state consistency
TEST(memory_manager_state_consistency) {
    setup_memory_system();
    teardown_memory_system();

    int initial_blocks = ss_mem_mgr.num_free_blocks;
    ASSERT_EQ(initial_blocks, 1);  // Should start with one large block

    // Allocate some memory
    uint32_t addr = ss_mem_alloc(1024);
    ASSERT_NOT_NULL(addr);

    // Free block count should remain the same (block shrunk, not removed)
    ASSERT_EQ(ss_mem_mgr.num_free_blocks, 1);

    // Remaining free block should have reduced size
    ASSERT_EQ(ss_mem_mgr.free_blocks[0].sz, 0x100000 - 1024);
    ASSERT_EQ(ss_mem_mgr.free_blocks[0].addr, 0x100000 + 1024);
}

// Test exact free block consumption
TEST(memory_exact_block_consumption) {
    setup_memory_system();

    // Set up a specific scenario: one 4K block
    ss_mem_mgr.num_free_blocks = 1;
    ss_mem_mgr.free_blocks[0].addr = 0x100000;
    ss_mem_mgr.free_blocks[0].sz = 4096;

    // Allocate exactly the size of the free block
    uint32_t addr = ss_mem_alloc(4096);
    ASSERT_EQ(addr, 0x100000);

    // Free block should be completely consumed (removed from list)
    ASSERT_EQ(ss_mem_mgr.num_free_blocks, 0);
}

// Test memory allocation statistics
TEST(memory_allocation_statistics) {
    setup_memory_system();
    teardown_memory_system();

    // Test total memory reporting
    uint32_t total_before = ss_mem_total_bytes();
    ASSERT_TRUE(total_before > 0);

    // Test free memory reporting
    uint32_t free_before = ss_mem_free_bytes();
    ASSERT_EQ(free_before, total_before);  // Initially all memory is free

    // Allocate some memory
    uint32_t alloc_size = 8192;
    uint32_t addr = ss_mem_alloc(alloc_size);
    ASSERT_NOT_NULL(addr);

    // Free memory should decrease
    uint32_t free_after = ss_mem_free_bytes();
    ASSERT_EQ(free_after, free_before - alloc_size);
}

// Run all memory tests
void run_memory_tests(void) {
    RUN_TEST(memory_alloc_basic);
    RUN_TEST(memory_alloc_zero_size);
    RUN_TEST(memory_alloc_4k_aligned);
    RUN_TEST(memory_alloc_out_of_memory);
    RUN_TEST(memory_alloc_fragmentation);
    RUN_TEST(memory_manager_state_consistency);
    RUN_TEST(memory_exact_block_consumption);
    RUN_TEST(memory_allocation_statistics);
}