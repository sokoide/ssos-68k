#pragma once
#include "task_manager.h"
#include <stdint.h>

/* common)
 * 0x000000-0x001FFF: Interrupt vector, IOCS work or etc (8KiB)
 *
 * disk boot mode)
 * 0x002000-0x0023FF: Boot sector (1KiB)
 * 0x0023FF-0x00FFFF: SSOS supervisor mode stack (55KiB)
 * 0x010000-0x02FFFF: SSOS .text (128KiB)
 * 0x030000-0x03FFFF: SSOS .data (64KiB)
 * 0x150000-0xBFFFFF: SSOS .ssos (10MiB)
 *
 * local mode)
 * supervisor stack, .text, .data, .bss auto assigned
 * app memory allocated by malloc()
 *
 * 1 memory chunk = 4KiB
 * 11MiB / 4Kib = 2816 chunks
 */

#define MEM_ALIGN_4K_MASK 0xfffff000

/**
 * @struct SsMemoryFreeBlock
 * @brief Represents a contiguous region of free memory in the system.
 *
 * This structure is the fundamental unit of the free memory tracking system.
 * It stores the location and size of an unallocated memory block.
 */
typedef struct {
    uint32_t start_address;  /**< Starting memory address of the free block */
    uint32_t size_in_bytes;  /**< Size of the free region in bytes */
} SsMemoryFreeBlock;

/**
 * @struct SsMemoryManager
 * @brief Global state of the SSOS Memory Management system.
 *
 * The memory manager maintains an array of free blocks, sorted by address.
 * This sorted array allows for efficient "Boundary Tag Coalescing" where
 * adjacent free blocks are merged to prevent memory fragmentation.
 */
typedef struct {
    int free_block_count;                          /**< Number of active free blocks in the list */
    SsMemoryFreeBlock free_blocks[MEM_FREE_BLOCKS]; /**< Array-based sorted list of free memory regions */
} SsMemoryManager;

extern void* ss_ssos_memory_base;
extern uint32_t ss_ssos_memory_size;
extern SsMemoryManager ss_mem_mgr;
extern uint8_t* ss_task_stack_base;

/**
 * @brief Initializes the global memory manager state with system RAM information.
 */
void ss_mem_init_info();

/**
 * @brief Retrieves the boundaries of the primary SSOS RAM region.
 */
void ss_mem_get_ssos_memory(void** base, uint32_t* sz);

/** @brief Retrieves the boundaries of the code (.text) section. */
void ss_mem_get_text(void** base, uint32_t* sz);

/** @brief Retrieves the boundaries of the initialized data (.data) section. */
void ss_mem_get_data(void** base, uint32_t* sz);

/** @brief Retrieves the boundaries of the zero-initialized data (.bss) section. */
void ss_mem_get_bss(void** base, uint32_t* sz);

/**
 * @brief Performs the initial setup of the memory pool.
 * This function marks the entire system RAM as one large free block.
 */
void ss_mem_init();

/**
 * @brief Releases a memory block back to the system.
 * 
 * @param addr The start address of the block to free.
 * @param sz The size of the block in bytes.
 * @return 0 on success, -1 on error.
 */
int ss_mem_free(uint32_t addr, uint32_t sz);

/**
 * @brief Releases a memory block, ensuring it is treated as 4KB aligned.
 */
int ss_mem_free4k(uint32_t addr, uint32_t sz);

/**
 * @brief Allocates a block of memory from the system pool.
 * Uses a First-Fit strategy to find the first suitable free block.
 *
 * @param sz The requested size in bytes.
 * @return The start address of the allocated block, or 0 if allocation failed.
 */
uint32_t ss_mem_alloc(uint32_t sz);

/**
 * @brief Allocates 4KB-aligned memory, rounding up the requested size if necessary.
 */
uint32_t ss_mem_alloc4k(uint32_t sz);

/** @brief Returns the total size of managed system RAM. */
uint32_t ss_mem_total_bytes();

/** @brief Returns the current total sum of all free memory fragments. */
uint32_t ss_mem_free_bytes();
