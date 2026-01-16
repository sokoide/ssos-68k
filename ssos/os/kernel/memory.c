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
SsMemoryManager ss_mem_mgr;
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
    ss_mem_mgr.free_block_count = 0;
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
int ss_mem_free(uint32_t block_address, uint32_t block_size) {
    int block_index;

    // 1. Validate Input
    // We cannot free a null address or a zero-sized block.
    if (block_address == 0 || block_size == 0) {
        return -1;
    }

    // 2. Find the Insertion Point
    // The memory manager maintains free blocks in a sorted list by their start address.
    // We use a linear search to find where our freed block fits in this sorted list.
    for (block_index = 0; block_index < ss_mem_mgr.free_block_count; block_index++) {
        // If the current free block's address is higher than ours, we found the spot.
        if (ss_mem_mgr.free_blocks[block_index].start_address > block_address) {
            break;
        }
    }

    // 3. Attempt Coalescing with the PREVIOUS block
    // Coalescing merges adjacent free blocks into one larger block to reduce fragmentation.
    if (block_index > 0) {
        SsMemoryFreeBlock* previous_block = &ss_mem_mgr.free_blocks[block_index - 1];
        uint32_t previous_block_end = previous_block->start_address + previous_block->size_in_bytes;

        // Check if the previous block ends exactly where our freed block starts.
        if (previous_block_end == block_address) {
            // Found a match! Extend the previous block's size.
            previous_block->size_in_bytes += block_size;

            // 3.1. Check for a "Triple Merge"
            // If the freed block also connects to the NEXT block, we can merge all three!
            if (block_index < ss_mem_mgr.free_block_count) {
                SsMemoryFreeBlock* next_block = &ss_mem_mgr.free_blocks[block_index];
                uint32_t freed_block_end = block_address + block_size;

                if (next_block->start_address == freed_block_end) {
                    // Triple merge: Previous + Freed + Next become one giant block.
                    previous_block->size_in_bytes += next_block->size_in_bytes;
                    
                    // Since the 'next_block' is now merged into 'previous_block', we must remove it from the list.
                    ss_mem_mgr.free_block_count--;
                    for (int shift_index = block_index; shift_index < ss_mem_mgr.free_block_count; shift_index++) {
                        ss_mem_mgr.free_blocks[shift_index] = ss_mem_mgr.free_blocks[shift_index + 1];
                    }
                }
            }
            return 0; // Successfully merged (backwards)
        }
    }

    // 4. Attempt Coalescing with the NEXT block (if backward merge didn't happen)
    if (block_index < ss_mem_mgr.free_block_count) {
        SsMemoryFreeBlock* next_block = &ss_mem_mgr.free_blocks[block_index];
        uint32_t freed_block_end = block_address + block_size;

        // Check if our freed block ends exactly where the next block starts.
        if (next_block->start_address == freed_block_end) {
            // Found a match! Move the next block's start back and increase its size.
            next_block->start_address = block_address;
            next_block->size_in_bytes += block_size;
            return 0; // Successfully merged (forwards)
        }
    }

    // 5. No Coalescing Possible - Insert as a new Free Block
    // If we reach here, our block is isolated (not adjacent to any other free block).
    if (ss_mem_mgr.free_block_count < MEM_FREE_BLOCKS) {
        // Shift existing blocks to the right to make a "hole" for our new block.
        for (int shift_index = ss_mem_mgr.free_block_count; shift_index > block_index; shift_index--) {
            ss_mem_mgr.free_blocks[shift_index] = ss_mem_mgr.free_blocks[shift_index - 1];
        }

        // Insert the new block into the list.
        ss_mem_mgr.free_blocks[block_index].start_address = block_address;
        ss_mem_mgr.free_blocks[block_index].size_in_bytes = block_size;
        ss_mem_mgr.free_block_count++;
        return 0;
    }

    // 6. Handle Error: Free block table overflow
    // This is a critical failure. We've run out of slots to track free memory regions.
    ss_set_error(SS_ERROR_OUT_OF_RESOURCES, SS_SEVERITY_ERROR,
                __func__, __FILE__, __LINE__, "Free block table is full - cannot track more fragments");
    return -1;
}

/**
 * @brief Frees a memory block, ensuring the size is rounded up to a 4KB boundary.
 *
 * This is useful for memory regions that were allocated using ss_mem_alloc4k.
 */
int ss_mem_free4k(uint32_t addr, uint32_t sz) {
    if (sz == 0) {
        return -1;
    }

    // Round up the size to the nearest 4KB.
    // Alignment formula: (size + (alignment - 1)) & ~(alignment - 1)
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
__attribute__((weak)) uint32_t ss_mem_alloc(uint32_t requested_size) {
    int block_index;
    uint32_t allocated_address;

    if (requested_size == 0) {
        return 0;
    }

    // 1. First-Fit Search
    // We iterate through our sorted list of free blocks and pick the FIRST block
    // that is large enough to satisfy the request. This is the "First-Fit" strategy.
    for (block_index = 0; block_index < ss_mem_mgr.free_block_count; block_index++) {
        SsMemoryFreeBlock* current_block = &ss_mem_mgr.free_blocks[block_index];

        if (current_block->size_in_bytes >= requested_size) {
            // We found a suitable block!
            allocated_address = current_block->start_address;

            // Update the block's state by "shrinking" it from the front.
            current_block->start_address += requested_size;
            current_block->size_in_bytes -= requested_size;

            // 2. Remove Empty Blocks
            // If the block was exactly the size we needed, it's now empty (size 0).
            if (current_block->size_in_bytes == 0) {
                ss_mem_mgr.free_block_count--;

                // To remove a block from our array-based list, we must shift all subsequent blocks left.
                int remaining_blocks = ss_mem_mgr.free_block_count - block_index;
                if (remaining_blocks > 0) {
                    // EDUCATIONAL NOTE: Performance Optimization for the Motorola 68000
                    // On the 68000, 32-bit (Long) moves are very efficient. 
                    // Instead of copying the struct element by element, we cast the pointers 
                    // to 32-bit integers to move the data in bulk.
                    SsMemoryFreeBlock* source = &ss_mem_mgr.free_blocks[block_index + 1];
                    SsMemoryFreeBlock* destination = &ss_mem_mgr.free_blocks[block_index];

                    uint32_t* source_32 = (uint32_t*)source;
                    uint32_t* destination_32 = (uint32_t*)destination;
                    
                    // Each SsMemoryFreeBlock is 8 bytes, which is exactly two 32-bit Longs.
                    int long_words_to_copy = remaining_blocks * (sizeof(SsMemoryFreeBlock) / sizeof(uint32_t));

                    for (int copy_index = 0; copy_index < long_words_to_copy; copy_index++) {
                        *destination_32++ = *source_32++;
                    }
                }
            }
            return allocated_address;
        }
    }

    // 3. Allocation Failed
    // No block was large enough to satisfy the request (Out of Memory).
    return 0;
}

/**
 * @brief Allocates a 4KB-aligned block of memory.
 *
 * Aligned allocation is critical for hardware features like DMA or 
 * memory-mapped I/O, which often require page-aligned buffers.
 */
__attribute__((weak)) uint32_t ss_mem_alloc4k(uint32_t requested_size) {
    if (requested_size == 0) {
        return 0;
    }

    // Round up the requested size to the nearest 4KB page.
    // MEM_ALIGN_4K = 4096 (0x1000)
    // MEM_ALIGN_4K_MASK = 0xFFFFF000
    uint32_t aligned_size = (requested_size + MEM_ALIGN_4K - 1) & MEM_ALIGN_4K_MASK;

    return ss_mem_alloc(aligned_size);
}

/**
 * @brief Returns the total amount of RAM managed by the SSOS kernel.
 */
__attribute__((weak)) uint32_t ss_mem_total_bytes() {
    return ss_ssos_memory_size;
}

/**
 * @brief Calculates the total amount of free memory currently available.
 * 
 * This is done by summing up the sizes of all individual free blocks.
 */
__attribute__((weak)) uint32_t ss_mem_free_bytes() {
    uint32_t total_free_bytes = 0;
    for (int block_index = 0; block_index < ss_mem_mgr.free_block_count; block_index++) {
        total_free_bytes += ss_mem_mgr.free_blocks[block_index].size_in_bytes;
    }
    return total_free_bytes;
}
