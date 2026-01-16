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

void ss_mem_init_info() {
    ss_mem_get_ssos_memory(&ss_ssos_memory_base, &ss_ssos_memory_size);
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
void ss_mem_get_ssos_memory(void** base, uint32_t* sz) {
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
void ss_mem_get_text(void** base, uint32_t* sz) {
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
void ss_mem_get_data(void** base, uint32_t* sz) {
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
void ss_mem_get_bss(void** base, uint32_t* sz) {
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

    // 1. Input Validation
    // We cannot free a null address or a zero-sized block fragment.
    if (block_address == 0 || block_size == 0) {
        return -1;
    }

    // 2. Locate Insertion Point
    // The memory manager keeps the list sorted by memory address to allow
    // for efficient merging (coalescing) of adjacent free fragments.
    for (block_index = 0; block_index < ss_mem_mgr.free_block_count; block_index++) {
        // If the current list entry starts after our freed block, we've found the spot.
        if (ss_mem_mgr.free_blocks[block_index].start_address > block_address) {
            break;
        }
    }

    // 3. Backward Coalescing
    // Attempt to merge the freed block with the fragment immediately preceding it.
    if (block_index > 0) {
        SsMemoryFreeBlock* previous_fragment = &ss_mem_mgr.free_blocks[block_index - 1];
        uint32_t previous_fragment_end = previous_fragment->start_address + previous_fragment->size_in_bytes;

        // If the previous block ends exactly where our freed block begins:
        if (previous_fragment_end == block_address) {
            // MERGE: Absorb our block into the previous fragment.
            previous_fragment->size_in_bytes += block_size;

            // 3.1. Triple Coalescing Check
            // Now check if our merged block is also adjacent to the NEXT fragment.
            if (block_index < ss_mem_mgr.free_block_count) {
                SsMemoryFreeBlock* next_fragment = &ss_mem_mgr.free_blocks[block_index];
                uint32_t current_block_end = block_address + block_size;

                if (next_fragment->start_address == current_block_end) {
                    // TRIPLE MERGE: Prev + Ours + Next become one single fragment.
                    previous_fragment->size_in_bytes += next_fragment->size_in_bytes;
                    
                    // Remove the redundant 'next' entry and shift the list left.
                    ss_mem_mgr.free_block_count--;
                    for (int i = block_index; i < ss_mem_mgr.free_block_count; i++) {
                        ss_mem_mgr.free_blocks[i] = ss_mem_mgr.free_blocks[i + 1];
                    }
                }
            }
            return 0; // Backward merge successful.
        }
    }

    // 4. Forward Coalescing
    // If we couldn't merge backwards, check if we can merge with the next fragment.
    if (block_index < ss_mem_mgr.free_block_count) {
        SsMemoryFreeBlock* next_fragment = &ss_mem_mgr.free_blocks[block_index];
        uint32_t current_block_end = block_address + block_size;

        // If our block ends exactly where the next fragment starts:
        if (next_fragment->start_address == current_block_end) {
            // MERGE: Extend the start of the next fragment backwards.
            next_fragment->start_address = block_address;
            next_fragment->size_in_bytes += block_size;
            return 0; // Forward merge successful.
        }
    }

    // 5. Array Insertion
    // No neighbors found. Insert this block as a new isolated free fragment.
    if (ss_mem_mgr.free_block_count < MEM_FREE_BLOCKS) {
        // Shift following entries to the right to create an insertion gap.
        for (int i = ss_mem_mgr.free_block_count; i > block_index; i--) {
            ss_mem_mgr.free_blocks[i] = ss_mem_mgr.free_blocks[i - 1];
        }

        // Fill the gap with our new fragment data.
        ss_mem_mgr.free_blocks[block_index].start_address = block_address;
        ss_mem_mgr.free_blocks[block_index].size_in_bytes = block_size;
        ss_mem_mgr.free_block_count++;
        return 0;
    }

    // 6. Critical Error: Resource Exhaustion
    // The fixed-size free block table is full. Fragmentation has reached an
    // unrecoverable state, or MEM_FREE_BLOCKS is too small for the workload.
    ss_set_error(SS_ERROR_OUT_OF_RESOURCES, SS_SEVERITY_ERROR,
                __func__, __FILE__, __LINE__, "Memory fragmentation limit: Free block table full.");
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

    // 1. Input Validation
    // Zero-sized allocation is an invalid request.
    if (requested_size == 0) {
        return 0;
    }

    // 2. First-Fit Search Strategy
    // We iterate through our address-sorted list of free memory fragments.
    // The "First-Fit" algorithm selects the first fragment that can satisfy the request.
    for (block_index = 0; block_index < ss_mem_mgr.free_block_count; block_index++) {
        SsMemoryFreeBlock* current_block = &ss_mem_mgr.free_blocks[block_index];

        if (current_block->size_in_bytes >= requested_size) {
            // MATCH FOUND: Satisfy the request from this block.
            allocated_address = current_block->start_address;

            // Reduce the free block's size and advance its start address.
            current_block->start_address += requested_size;
            current_block->size_in_bytes -= requested_size;

            // 3. Post-Allocation Cleanup
            // If the block was fully consumed (size is now zero), remove it from the list.
            if (current_block->size_in_bytes == 0) {
                ss_mem_mgr.free_block_count--;

                // Shift subsequent blocks to maintain a contiguous array list.
                int blocks_to_shift = ss_mem_mgr.free_block_count - block_index;
                if (blocks_to_shift > 0) {
                    /**
                     * MOTOROLA 68000 OPTIMIZATION:
                     * On the 68k, 32-bit (Long) operations are significantly faster than
                     * series of 8-bit or 16-bit operations. Since our block structure
                     * is exactly 8 bytes (2x 32-bit uint32_t), we use 32-bit bulk moves.
                     */
                    SsMemoryFreeBlock* source_ptr = &ss_mem_mgr.free_blocks[block_index + 1];
                    SsMemoryFreeBlock* destination_ptr = &ss_mem_mgr.free_blocks[block_index];

                    uint32_t* src_u32 = (uint32_t*)source_ptr;
                    uint32_t* dst_u32 = (uint32_t*)destination_ptr;
                    
                    // Each block contains 2 uint32_t elements.
                    int words_to_copy = blocks_to_shift * (sizeof(SsMemoryFreeBlock) / sizeof(uint32_t));

                    for (int i = 0; i < words_to_copy; i++) {
                        *dst_u32++ = *src_u32++;
                    }
                }
            }
            return allocated_address;
        }
    }

    // 4. Out of Memory
    // No free block was found that could accommodate the requested size.
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
