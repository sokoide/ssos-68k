#include "../framework/ssos_test.h"

// Include SSOS memory system (LOCAL_MODE enabled in Makefile)
#include "memory.h"
#include "ss_config.h"

// Test helper: Initialize memory system for testing
static void setup_memory_system(void) {
    // Initialize memory region information first
    ss_mem_init_info();
    // Then initialize memory manager with that information
    ss_mem_init();
}

// Test helper: Reset memory system
static void teardown_memory_system(void) {
    // Reset memory manager state
    ss_mem_mgr.free_block_count = 0;

    // Add one large free block for fresh start
    ss_mem_mgr.free_blocks[0].start_address = 0x100000;  // Test memory base
    ss_mem_mgr.free_blocks[0].size_in_bytes = 0x100000;    // 1MB for testing
    ss_mem_mgr.free_block_count = 1;
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

    ASSERT_NEQ(addr1, 0);
    ASSERT_NEQ(addr2, 0);
    ASSERT_NEQ(addr3, 0);
    ASSERT_NEQ(addr4, 0);

    // All addresses should be 4K-aligned
    ASSERT_ALIGNED_4K(addr1);
    ASSERT_ALIGNED_4K(addr2);
    ASSERT_ALIGNED_4K(addr3);
    ASSERT_ALIGNED_4K(addr4);

    // Verify allocations are distinct (no overlap)
    ASSERT_NEQ(addr1, addr2);
    ASSERT_NEQ(addr2, addr3);
    ASSERT_NEQ(addr3, addr4);
    ASSERT_NEQ(addr1, addr3);
    ASSERT_NEQ(addr1, addr4);
    ASSERT_NEQ(addr2, addr4);
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

    ASSERT_NEQ(addr1, 0);
    ASSERT_NEQ(addr2, 0);
    ASSERT_NEQ(addr3, 0);

    // Verify they are sequential as expected with our allocator
    ASSERT_EQ(addr2, addr1 + 1024);
    ASSERT_EQ(addr3, addr2 + 2048);
}

// Test memory manager state consistency
TEST(memory_manager_state_consistency) {
    setup_memory_system();
    teardown_memory_system();

    int initial_blocks = ss_mem_mgr.free_block_count;
    ASSERT_EQ(initial_blocks, 1);  // Should start with one large block

    // Allocate some memory
    uint32_t addr = ss_mem_alloc(1024);
    ASSERT_NEQ(addr, 0);

    // Free block count should remain the same (block shrunk, not removed)
    ASSERT_EQ(ss_mem_mgr.free_block_count, 1);

    // Remaining free block should have reduced size
    ASSERT_EQ(ss_mem_mgr.free_blocks[0].size_in_bytes, 0x100000 - 1024);
    ASSERT_EQ(ss_mem_mgr.free_blocks[0].start_address, 0x100000 + 1024);
}

// Test exact free block consumption
TEST(memory_exact_block_consumption) {
    setup_memory_system();

    // Set up a specific scenario: one 4K block
    ss_mem_mgr.free_block_count = 1;
    ss_mem_mgr.free_blocks[0].start_address = 0x100000;
    ss_mem_mgr.free_blocks[0].size_in_bytes = 4096;

    // Allocate exactly the size of the free block
    uint32_t addr = ss_mem_alloc(4096);
    ASSERT_NEQ(addr, 0);  // Should get valid memory, not specific address

    // Free block should be completely consumed (removed from list)
    ASSERT_EQ(ss_mem_mgr.free_block_count, 0);
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
    ASSERT_NEQ(addr, 0);

    // Free memory should decrease
    uint32_t free_after = ss_mem_free_bytes();
    ASSERT_EQ(free_after, free_before - alloc_size);
}

// Test backward coalescing (merging with previous block)
TEST(memory_coalesce_backward) {
    setup_memory_system();
    teardown_memory_system();
    
    // Layout: [Alloc1 (1K)][Alloc2 (1K)][Alloc3 (1K)][Free...]
    uint32_t addr1 = ss_mem_alloc(1024); 
    uint32_t addr2 = ss_mem_alloc(1024);
    uint32_t addr3 = ss_mem_alloc(1024); // Wall to prevent forward merge with trailing free block
    
    // Free 1. List: [0x100000, 1K], [Trailing Free] (Count: 2)
    ss_mem_free(addr1, 1024);
    ASSERT_EQ(ss_mem_mgr.free_block_count, 2);
    
    // Free 2. Should merge with 1.
    ss_mem_free(addr2, 1024);
    
    // Should be coalesced into one block at the start
    ASSERT_EQ(ss_mem_mgr.free_block_count, 2); 
    ASSERT_EQ(ss_mem_mgr.free_blocks[0].start_address, 0x100000);
    ASSERT_EQ(ss_mem_mgr.free_blocks[0].size_in_bytes, 2048);
}

// Test forward coalescing (merging with next block)
TEST(memory_coalesce_forward) {
    setup_memory_system();
    teardown_memory_system();
    
    // Layout: [Alloc1 (1K)][Alloc2 (1K)][Alloc3 (1K)][Free...]
    uint32_t addr1 = ss_mem_alloc(1024);
    uint32_t addr2 = ss_mem_alloc(1024);
    uint32_t addr3 = ss_mem_alloc(1024); // Wall
    
    // Free 2. List: [0x100400, 1K], [Trailing Free] (Count: 2)
    ss_mem_free(addr2, 1024);
    ASSERT_EQ(ss_mem_mgr.free_block_count, 2);
    
    // Free 1. Should merge FORWARD into 2.
    ss_mem_free(addr1, 1024);
    
    ASSERT_EQ(ss_mem_mgr.free_block_count, 2);
    ASSERT_EQ(ss_mem_mgr.free_blocks[0].start_address, 0x100000);
    ASSERT_EQ(ss_mem_mgr.free_blocks[0].size_in_bytes, 2048);
}

// Test triple coalescing (Prev + Current + Next)
TEST(memory_coalesce_triple) {
    setup_memory_system();
    teardown_memory_system();
    
    // Layout: [Alloc1][Alloc2][Alloc3][Alloc4][Free...]
    uint32_t addr1 = ss_mem_alloc(1024);
    uint32_t addr2 = ss_mem_alloc(1024);
    uint32_t addr3 = ss_mem_alloc(1024);
    uint32_t addr4 = ss_mem_alloc(1024); // Wall
    
    // Free 1 and 3. 
    ss_mem_free(addr1, 1024);
    ss_mem_free(addr3, 1024);
    
    // List: [0x100000, 1K], [0x100800, 1K], [Trailing Free]
    ASSERT_EQ(ss_mem_mgr.free_block_count, 3);
    
    // Free 2. Should merge with both 1 and 3.
    ss_mem_free(addr2, 1024);
    
    // Result: [0x100000, 3K], [Trailing Free]
    ASSERT_EQ(ss_mem_mgr.free_block_count, 2);
    ASSERT_EQ(ss_mem_mgr.free_blocks[0].size_in_bytes, 3072);
}

// Test freeing an isolated block (no neighbors)
TEST(memory_free_isolated) {
    setup_memory_system();
    teardown_memory_system();
    
    // Layout: [Alloc1][Alloc2][Alloc3][Alloc4][Free...]
    uint32_t addr1 = ss_mem_alloc(1024);
    uint32_t addr2 = ss_mem_alloc(1024);
    uint32_t addr3 = ss_mem_alloc(1024);
    uint32_t addr4 = ss_mem_alloc(1024); // Wall
    
    // Free 2. Neighbors (1 and 3) are still allocated.
    ss_mem_free(addr2, 1024);
    
    // List: [0x100400, 1K], [Trailing Free]
    ASSERT_EQ(ss_mem_mgr.free_block_count, 2);
    ASSERT_EQ(ss_mem_mgr.free_blocks[0].start_address, 0x100400);
}

// Test invalid free parameters
TEST(memory_free_invalid) {
    setup_memory_system();
    
    int result1 = ss_mem_free(0, 1024);
    ASSERT_EQ(result1, -1);
    
    int result2 = ss_mem_free(0x100000, 0);
    ASSERT_EQ(result2, -1);
}

// Test first-fit selection logic
TEST(memory_first_fit_selection) {
    setup_memory_system();
    teardown_memory_system();
    
    // Create fragments: [Free 1K][Alloc 64][Free 4K][Alloc 64][Free 2K][Alloc 64][Trailing]
    uint32_t h1 = ss_mem_alloc(1024);
    ss_mem_alloc(64);
    uint32_t h2 = ss_mem_alloc(4096);
    ss_mem_alloc(64);
    uint32_t h3 = ss_mem_alloc(2048);
    ss_mem_alloc(64);
    
    ss_mem_free(h1, 1024);
    ss_mem_free(h2, 4096);
    ss_mem_free(h3, 2048);
    
    // Request 1.5K. First block (1K) is too small. Second (4K) should be used.
    uint32_t addr = ss_mem_alloc(1536);
    ASSERT_EQ(addr, h2);
}

// Test free block table full scenario
TEST(memory_table_full) {
    setup_memory_system();
    teardown_memory_system();
    
    // To fill the table, we must have MEM_FREE_BLOCKS entries.
    // 1 is already used for the trailing free space.
    // So we need to create MEM_FREE_BLOCKS - 1 more fragments.
    
    static uint32_t allocs[MEM_FREE_BLOCKS];
    
    // 1. Allocate many small blocks with walls
    for (int i = 0; i < MEM_FREE_BLOCKS - 1; i++) {
        allocs[i] = ss_mem_alloc(16);
        ss_mem_alloc(16); // wall
    }
    
    // 2. Free them all to create separate fragments
    for (int i = 0; i < MEM_FREE_BLOCKS - 1; i++) {
        ss_mem_free(allocs[i], 16);
    }
    
    // Total fragments = (MEM_FREE_BLOCKS - 1) + 1 (trailing) = 1024
    ASSERT_EQ(ss_mem_mgr.free_block_count, MEM_FREE_BLOCKS);
    
    // 3. Try to create one more fragment (isolated)
    // IMPORTANT: Request a size larger than 16 bytes to ensure we don't 
    // reuse and remove any of the existing 16-byte fragments.
    uint32_t extra_wall = ss_mem_alloc(1024); 
    uint32_t extra_hole = ss_mem_alloc(1024);
    ss_mem_alloc(1024); // another wall
    
    // At this point count is still 1024 because we split the trailing block
    // but didn't remove any of the small 16-byte fragments.
    ASSERT_EQ(ss_mem_mgr.free_block_count, MEM_FREE_BLOCKS);
    
    // Freeing extra_hole should fail because there are no slots left to insert it.
    int result = ss_mem_free(extra_hole, 1024);
    ASSERT_EQ(result, -1);
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
    RUN_TEST(memory_coalesce_backward);
    RUN_TEST(memory_coalesce_forward);
    RUN_TEST(memory_coalesce_triple);
    RUN_TEST(memory_free_isolated);
    RUN_TEST(memory_free_invalid);
    RUN_TEST(memory_first_fit_selection);
    RUN_TEST(memory_table_full);
}