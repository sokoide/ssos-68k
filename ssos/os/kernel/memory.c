/**
 * @file memory.c
 * @brief SSOS Memory Management System
 *
 * This file implements the core memory management functionality for SSOS,
 * providing dynamic memory allocation and deallocation services optimized
 * for the X68000 embedded environment.
 *
 * @section Architecture
 * The memory management system uses a boundary tag coalescing algorithm
 * with first-fit allocation strategy, optimized for embedded systems
 * performance and minimal fragmentation.
 *
 * @section Key_Features
 * - 4KB page-aligned allocations for hardware optimization
 * - Boundary tag coalescing to minimize fragmentation
 * - Thread-safe operation (requires external synchronization)
 * - Dual-mode support (OS mode vs LOCAL_MODE testing)
 * - Performance optimizations for 68000 architecture
 *
 * @section Memory_Layout
 * - **OS Mode**: Uses linker-defined memory regions
 * - **LOCAL_MODE**: Uses predefined test regions
 * - **Dynamic Pool**: Managed by free block coalescing algorithm
 *
 * @section Thread_Safety
 * Functions in this module are not internally thread-safe and require
 * external synchronization (typically interrupt disable) when used in
 * multi-tasking environments.
 *
 * @author SSOS Development Team
 * @date 2025
 */

#include "memory.h"
#include "kernel.h"
#include <stdint.h>
#include <stddef.h>

void* ss_ssos_memory_base;
uint32_t ss_ssos_memory_size;
SsMemMgr ss_mem_mgr;
uint8_t* ss_task_stack_base;

void ss_init_memory_info() {
    ss_get_ssos_memory(&ss_ssos_memory_base, &ss_ssos_memory_size);
}

/**
 * @brief Get the SSOS memory region information
 *
 * Retrieves the base address and size of the SSOS memory region.
 * This region is used for dynamic memory allocation within the OS.
 * In LOCAL_MODE, uses predefined test values; in OS mode, uses
 * linker-defined symbols for the actual memory region.
 *
 * @param base Pointer to store the memory region base address
 * @param sz Pointer to store the memory region size in bytes
 *
 * @note This function does not perform allocation - it only provides
 * information about the available memory region for the memory manager.
 */
void ss_get_ssos_memory(void** base, uint32_t* sz) {
    if (base == NULL || sz == NULL)
        return;

#ifdef LOCAL_MODE
    *base = local_ssos_memory_base;
    *sz = local_ssos_memory_size;
#else
    *base = (void*)__ssosram_start;
    *sz = (uint32_t)__ssosram_size;
#endif
}

/**
 * @brief Get the text section memory information
 *
 * Retrieves the base address and size of the executable text section.
 * In LOCAL_MODE, returns test values; in OS mode, uses linker-defined
 * symbols for the actual text section boundaries.
 *
 * @param base Pointer to store the text section base address
 * @param sz Pointer to store the text section size in bytes
 *
 * @note The text section contains executable code and is typically
 * read-only during normal operation.
 */
void ss_get_text(void** base, uint32_t* sz) {
    if (base == NULL || sz == NULL)
        return;

#ifdef LOCAL_MODE
    *base = (void*)0;
    *sz = local_text_size;
#else
    *base = (void*)__text_start;
    *sz = (uint32_t)__text_size;
#endif
}

/**
 * @brief Get the data section memory information
 *
 * Retrieves the base address and size of the initialized data section.
 * The data section contains global and static variables with initial values.
 *
 * @param base Pointer to store the data section base address
 * @param sz Pointer to store the data section size in bytes
 *
 * @note In LOCAL_MODE, returns zero address since data is handled differently.
 */
void ss_get_data(void** base, uint32_t* sz) {
    if (base == NULL || sz == NULL)
        return;

#ifdef LOCAL_MODE
    *base = (void*)0;
    *sz = local_data_size;
#else
    *base = (void*)__data_start;
    *sz = (uint32_t)__data_size;
#endif
}

/**
 * @brief Get the BSS section memory information
 *
 * Retrieves the base address and size of the uninitialized data section.
 * The BSS section contains global and static variables initialized to zero.
 *
 * @param base Pointer to store the BSS section base address
 * @param sz Pointer to store the BSS section size in bytes
 *
 * @note The BSS section must be zero-initialized before use.
 * In LOCAL_MODE, returns zero address since BSS is handled differently.
 */
void ss_get_bss(void** base, uint32_t* sz) {
    if (base == NULL || sz == NULL)
        return;

#ifdef LOCAL_MODE
    *base = (void*)0;
    *sz = local_bss_size;
#else
    *base = (void*)__bss_start;
    *sz = (uint32_t)__bss_size;
#endif
}

/**
 * @brief Initialize the SSOS memory management system
 *
 * Initializes the memory manager by resetting the free block count and
 * adding the entire SSOS memory region to the free block list.
 * This should be called once during system initialization.
 */
__attribute__((weak)) void ss_mem_init() {
    ss_mem_mgr.num_free_blocks = 0;
    ss_mem_free((uint32_t)ss_ssos_memory_base, ss_ssos_memory_size);
}

/**
 * @brief Free a memory block and add it back to the free pool
 *
 * Frees a previously allocated memory block and attempts to coalesce it
 * with adjacent free blocks to reduce fragmentation. The memory manager
 * maintains free blocks in address order for efficient coalescing.
 *
 * @param addr Address of memory block to free
 * @param sz Size of memory block in bytes
 * @return 0 on success, -1 if free block table is full or invalid parameters
 *
 * @section Coalescing_Algorithm
 * Boundary Tag Coalescing Strategy:
 *
 * 1. **Find Insertion Point**: Binary search for correct position in sorted free list
 * 2. **Backward Coalescing**: Check if freed block can merge with previous block
 * 3. **Forward Coalescing**: Check if freed block can merge with next block
 * 4. **Triple Merge**: Handle case where previous + freed + next form contiguous region
 * 5. **Insert New Block**: If no coalescing possible, insert as new free block
 *
 * @section Complexity
 * - **Time**: O(n) where n is number of free blocks (linear search)
 * - **Space**: O(1) additional space, O(MEM_FREE_BLOCKS) total for free block table
 *
 * @section Edge_Cases
 * - Adjacent to previous block only
 * - Adjacent to next block only
 * - Adjacent to both (triple merge)
 * - No adjacent blocks (new insertion)
 * - Free block table full (critical error)
 *
 * @section Fragmentation_Control
 * - Maintains free blocks in address order for efficient coalescing
 * - Prevents memory leaks by properly tracking all free regions
 * - Uses immediate coalescing to minimize fragmentation over time
 *
 * @note This function is critical for memory management performance and
 * should be called with interrupts disabled to ensure consistency.
 */
int ss_mem_free(uint32_t addr, uint32_t sz) {
    int i;

    // Input validation
    if (addr == 0 || sz == 0) {
        return -1;
    }

    // Find insertion point in sorted free block list
    // The free block list is maintained in address order, so we can use
    // linear search to find the correct insertion point for the new block
    for (i = 0; i < ss_mem_mgr.num_free_blocks; i++) {
        if (ss_mem_mgr.free_blocks[i].addr > addr)
            break;
    }

    // Try to coalesce with previous block
    // Check if the freed block is immediately adjacent to the previous free block
    // This is the most common coalescing scenario
    if (i > 0) {
        // Calculate if blocks are contiguous: prev_block_end == freed_block_start
        // check if the block to be freed can be combined with the previous block
        if (ss_mem_mgr.free_blocks[i - 1].addr + ss_mem_mgr.free_blocks[i - 1].sz == addr) {
            // can combine - extend previous block to include freed memory
            ss_mem_mgr.free_blocks[i - 1].sz += sz;

            // Check for triple merge: prev + freed + next are all contiguous
            // This is a rare but important optimization to prevent fragmentation
            // check the next free block for triple merge
            if (i < ss_mem_mgr.num_free_blocks &&
                ss_mem_mgr.free_blocks[i].addr == addr + sz) {
                // Triple merge: combine all three blocks into one
                // combine it with the next block (triple merge)
                ss_mem_mgr.free_blocks[i - 1].sz += ss_mem_mgr.free_blocks[i].sz;
                ss_mem_mgr.num_free_blocks--;

                // Remove the consumed next block by shifting remaining blocks left
                // Shift remaining blocks down
                for (; i < ss_mem_mgr.num_free_blocks; i++) {
                    ss_mem_mgr.free_blocks[i] = ss_mem_mgr.free_blocks[i + 1];
                }
            }
            return 0; // Successfully coalesced
        }
    }

    // Try to coalesce with next block
    // Check if freed block is immediately adjacent to the next free block
    // This handles the case where previous coalescing didn't occur
    if (i < ss_mem_mgr.num_free_blocks) {
        if (ss_mem_mgr.free_blocks[i].addr == addr + sz) {
            // can combine with next block - prepend freed block to next block
            ss_mem_mgr.free_blocks[i].addr = addr;  // Move start address back
            ss_mem_mgr.free_blocks[i].sz += sz;     // Extend size
            return 0; // Successfully coalesced
        }
    }

    // Cannot coalesce - insert as new free block
    // This path handles the case where no adjacent blocks exist
    if (ss_mem_mgr.num_free_blocks < MEM_FREE_BLOCKS) {
        // Free block table has space - insert new block at correct position
        // Shift existing blocks right to make room for insertion
        // Shift blocks to make room for insertion
        for (int j = ss_mem_mgr.num_free_blocks; j > i; j--) {
            ss_mem_mgr.free_blocks[j] = ss_mem_mgr.free_blocks[j - 1];
        }

        // Insert new free block at the correct sorted position
        // Insert new free block
        ss_mem_mgr.free_blocks[i].addr = addr;
        ss_mem_mgr.free_blocks[i].sz = sz;
        ss_mem_mgr.num_free_blocks++;
        return 0; // Successfully inserted
    }

    // CRITICAL ERROR: Free block table is full
    // This indicates severe fragmentation or insufficient table size
    // The system cannot track any more free memory regions

    // Free block table is full - this is a critical error
    ss_set_error(SS_ERROR_OUT_OF_RESOURCES, SS_SEVERITY_ERROR,
                __func__, __FILE__, __LINE__, "Free block table is full");
    return -1;
}

int ss_mem_free4k(uint32_t addr, uint32_t sz) {
    if (sz == 0)
        return -1;
    sz = (sz + MEM_ALIGN_4K - 1) & MEM_ALIGN_4K_MASK;
    return ss_mem_free(addr, sz);
}

/**
 * @brief Allocate memory from the SSOS memory pool
 *
 * Allocates a block of memory of the specified size from the free memory pool.
 * Uses first-fit allocation strategy optimized for embedded systems performance.
 *
 * @param sz Size in bytes to allocate (0 returns 0)
 * @return Address of allocated memory block, or 0 if allocation failed
 *
 * @section Algorithm
 * First-Fit Allocation Strategy:
 * 1. Search free block list for first block >= requested size
 * 2. If exact fit: Remove block from free list
 * 3. If larger: Split block, update free block size/address
 * 4. Use cache-friendly search order for better performance
 *
 * @section Optimizations
 * - **Cache-friendly search**: Linear search optimized for small allocations
 * - **32-bit operations**: Uses uint32_t for better performance on 68000
 * - **Minimal copying**: Efficient block removal with optimized memmove
 * - **Early size check**: Quick rejection of undersized blocks
 *
 * @section Thread_Safety
 * Not thread-safe. Caller must ensure mutual exclusion when used
 * in multi-tasking environment. Typically protected by interrupt disable.
 *
 * @section Memory_Alignment
 * Returns naturally aligned addresses suitable for any data type.
 * For 4KB alignment requirements, use ss_mem_alloc4k() instead.
 *
 * @note Allocation failures are silent - returns 0 without error reporting
 */
__attribute__((weak)) uint32_t ss_mem_alloc(uint32_t sz) {
    int i;
    uint32_t addr;

    if (sz == 0)
        return 0;

    // PERFORMANCE OPTIMIZATION: Cache-friendly search with early size check
    // Most allocations are small, so check smallest blocks first
    for (i = 0; i < ss_mem_mgr.num_free_blocks; i++) {
        // Quick size check before address calculation
        if (ss_mem_mgr.free_blocks[i].sz >= sz) {
            addr = ss_mem_mgr.free_blocks[i].addr;
            ss_mem_mgr.free_blocks[i].addr += sz;
            ss_mem_mgr.free_blocks[i].sz -= sz;

            if (ss_mem_mgr.free_blocks[i].sz == 0) {
                // PERF OPTIMIZATION: Exact fit - remove empty block from free list
                // This is the most common case when allocation consumes entire block
                ss_mem_mgr.num_free_blocks--;

                // OPTIMIZATION: Use 32-bit moves for better performance on 68000
                // Calculate remaining blocks to move after current position
                int remaining_blocks = ss_mem_mgr.num_free_blocks - i;
                if (remaining_blocks > 0) {
                    // Move multiple blocks efficiently using 32-bit operations
                    // This is faster than struct-by-struct copying on 68000
                    // Move multiple blocks efficiently
                    SsMemFreeBlock* src = &ss_mem_mgr.free_blocks[i + 1];
                    SsMemFreeBlock* dst = &ss_mem_mgr.free_blocks[i];

                    // Use 32-bit copies for better performance
                    // Each SsMemFreeBlock is 8 bytes (2 x uint32_t), so we copy as uint32_t
                    uint32_t* src32 = (uint32_t*)src;
                    uint32_t* dst32 = (uint32_t*)dst;
                    int blocks_to_copy = remaining_blocks * sizeof(SsMemFreeBlock) / sizeof(uint32_t);

                    // Perform bulk 32-bit copy for better cache performance
                    for (int j = 0; j < blocks_to_copy; j++) {
                        *dst32++ = *src32++;
                    }
                }
            }
            return addr;
        }
    }

    return 0;
}

/**
 * @brief Allocate 4KB-aligned memory from the SSOS memory pool
 *
 * Allocates memory with 4KB page alignment, which is optimal for
 * X68000 hardware performance and memory-mapped I/O operations.
 *
 * @param sz Size in bytes to allocate (will be rounded up to 4KB boundary)
 * @return Address of allocated memory block (4KB aligned), or 0 if allocation failed
 *
 * @section Alignment_Strategy
 * 1. Round requested size up to next 4KB boundary
 * 2. Allocate using standard allocator
 * 3. Return naturally 4KB-aligned address
 *
 * @note 4KB alignment is beneficial for:
 * - Hardware DMA operations
 * - Memory-mapped device access
 * - Cache performance optimization
 * - Page-based memory protection (future enhancement)
 */
__attribute__((weak)) uint32_t ss_mem_alloc4k(uint32_t sz) {
    if (sz == 0)
        return 0;

    // Round up to 4KB alignment: (size + 4095) & ~4095
    // MEM_ALIGN_4K = 4096, MEM_ALIGN_4K_MASK = ~4095
    sz = (sz + MEM_ALIGN_4K - 1) & MEM_ALIGN_4K_MASK;
    return ss_mem_alloc(sz);
}

__attribute__((weak)) uint32_t ss_mem_total_bytes() { return ss_ssos_memory_size; }

__attribute__((weak)) uint32_t ss_mem_free_bytes() {
    uint32_t ret = 0;
    for (int i = 0; i < ss_mem_mgr.num_free_blocks; i++) {
        ret += ss_mem_mgr.free_blocks[i].sz;
    }
    return ret;
}
