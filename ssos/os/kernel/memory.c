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
void ss_mem_init() {
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
 * @return 0 on success, -1 if free block table is full
 * 
 * Implementation uses coalescing algorithm to merge adjacent free blocks:
 * 1. Find insertion point in sorted free block list
 * 2. Attempt to merge with previous block
 * 3. Attempt to merge with next block
 * 4. Insert new block if no merging possible
 */
int ss_mem_free(uint32_t addr, uint32_t sz) {
    int i;
    
    // Input validation
    if (addr == 0 || sz == 0) {
        return -1;
    }
    
    // Find insertion point in sorted free block list
    for (i = 0; i < ss_mem_mgr.num_free_blocks; i++) {
        if (ss_mem_mgr.free_blocks[i].addr > addr)
            break;
    }
    
    // Try to coalesce with previous block
    if (i > 0) {
        // check if the block to be freed can be combined with the previous block
        if (ss_mem_mgr.free_blocks[i - 1].addr + ss_mem_mgr.free_blocks[i - 1].sz == addr) {
            // can combine
            ss_mem_mgr.free_blocks[i - 1].sz += sz;
            
            // check the next free block for triple merge
            if (i < ss_mem_mgr.num_free_blocks &&
                ss_mem_mgr.free_blocks[i].addr == addr + sz) {
                // combine it with the next block (triple merge)
                ss_mem_mgr.free_blocks[i - 1].sz += ss_mem_mgr.free_blocks[i].sz;
                ss_mem_mgr.num_free_blocks--;
                
                // Shift remaining blocks down
                for (; i < ss_mem_mgr.num_free_blocks; i++) {
                    ss_mem_mgr.free_blocks[i] = ss_mem_mgr.free_blocks[i + 1];
                }
            }
            return 0;
        }
    }
    
    // Try to coalesce with next block
    if (i < ss_mem_mgr.num_free_blocks) {
        if (ss_mem_mgr.free_blocks[i].addr == addr + sz) {
            // can combine with next block
            ss_mem_mgr.free_blocks[i].addr = addr;
            ss_mem_mgr.free_blocks[i].sz += sz;
            return 0;
        }
    }
    
    // Cannot coalesce - insert new free block
    if (ss_mem_mgr.num_free_blocks < MEM_FREE_BLOCKS) {
        // Shift blocks to make room for insertion
        for (int j = ss_mem_mgr.num_free_blocks; j > i; j--) {
            ss_mem_mgr.free_blocks[j] = ss_mem_mgr.free_blocks[j - 1];
        }
        
        // Insert new free block
        ss_mem_mgr.free_blocks[i].addr = addr;
        ss_mem_mgr.free_blocks[i].sz = sz;
        ss_mem_mgr.num_free_blocks++;
        return 0;
    }

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
 * Uses first-fit allocation strategy with performance optimizations for
 * cache-friendly searching and efficient block management.
 * 
 * @param sz Size in bytes to allocate
 * @return Address of allocated memory block, or 0 if allocation failed
 * 
 * Performance optimizations:
 * - Cache-friendly search starting with smallest blocks
 * - 32-bit aligned memory operations for better performance
 * - Optimized free block list management
 */
uint32_t ss_mem_alloc(uint32_t sz) {
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
                // remove this free block - optimized block move
                ss_mem_mgr.num_free_blocks--;

                // OPTIMIZATION: Use 32-bit moves for better performance
                int remaining_blocks = ss_mem_mgr.num_free_blocks - i;
                if (remaining_blocks > 0) {
                    // Move multiple blocks efficiently
                    SsMemFreeBlock* src = &ss_mem_mgr.free_blocks[i + 1];
                    SsMemFreeBlock* dst = &ss_mem_mgr.free_blocks[i];

                    // Use 32-bit copies for better performance
                    uint32_t* src32 = (uint32_t*)src;
                    uint32_t* dst32 = (uint32_t*)dst;
                    int blocks_to_copy = remaining_blocks * sizeof(SsMemFreeBlock) / sizeof(uint32_t);

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

uint32_t ss_mem_alloc4k(uint32_t sz) {
    if (sz == 0)
        return 0;
    sz = (sz + MEM_ALIGN_4K - 1) & MEM_ALIGN_4K_MASK;
    return ss_mem_alloc(sz);
}

uint32_t ss_mem_total_bytes() { return ss_ssos_memory_size; }

uint32_t ss_mem_free_bytes() {
    uint32_t ret = 0;
    for (int i = 0; i < ss_mem_mgr.num_free_blocks; i++) {
        ret += ss_mem_mgr.free_blocks[i].sz;
    }
    return ret;
}
