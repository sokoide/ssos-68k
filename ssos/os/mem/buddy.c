#include "memory.h"
#include <stdint.h>
#include <string.h>

static SSBuddySystem buddy;

static int order_to_index(uint8_t order) {
    return order - SS_BUDDY_MIN_ORDER;
}

static uint32_t order_size(uint8_t order) {
    return (uint32_t)1 << order;
}

static uint32_t block_index(void* ptr) {
    return ((uint8_t*)ptr - (uint8_t*)buddy.base) >> SS_BUDDY_MIN_ORDER;
}

static void* block_buddy(void* ptr, uint8_t order) {
    uint32_t offset = (uint8_t*)ptr - (uint8_t*)buddy.base;
    uint32_t buddy_offset = offset ^ order_size(order);
    return (uint8_t*)buddy.base + buddy_offset;
}

void ss_mem_init(void* base, uint32_t size) {
    uint32_t i;

    memset(&buddy, 0, sizeof(buddy));
    buddy.base = base;
    buddy.total_size = size;

    /* Align size down to minimum block size */
    size &= ~((uint32_t)order_size(SS_BUDDY_MIN_ORDER) - 1);
    buddy.map_entries = size >> SS_BUDDY_MIN_ORDER;

    /* Use first portion of memory for order map */
    uint32_t map_size = buddy.map_entries;
    buddy.order_map = (uint8_t*)base;
    memset(buddy.order_map, 0xFF, map_size);

    /* Usable memory starts after the map */
    uint32_t usable_start = ((map_size + order_size(SS_BUDDY_MIN_ORDER) - 1)
                             & ~(order_size(SS_BUDDY_MIN_ORDER) - 1));
    uint32_t usable_size = size - usable_start;
    uint8_t* usable_base = (uint8_t*)base + usable_start;

    /* Find largest order that fits */
    uint8_t max_order = SS_BUDDY_MAX_ORDER;
    while (max_order > SS_BUDDY_MIN_ORDER && order_size(max_order) > usable_size) {
        max_order--;
    }

    /* Split from top order down */
    for (i = 0; i < SS_BUDDY_ORDERS; i++) {
        buddy.free_lists[i] = NULL;
    }

    /* Add the entire usable region as blocks of the max possible order */
    uint32_t offset = 0;
    while (offset + order_size(max_order) <= usable_size) {
        SSBuddyBlock* blk = (SSBuddyBlock*)(usable_base + offset);
        int idx = order_to_index(max_order);
        blk->next = buddy.free_lists[idx];
        blk->order = max_order;
        buddy.free_lists[idx] = blk;
        buddy.order_map[block_index(blk)] = max_order;
        offset += order_size(max_order);
    }
}

static void split_block(SSBuddyBlock* blk, uint8_t from_order, uint8_t to_order) {
    uint8_t order = from_order;
    while (order > to_order) {
        order--;
        SSBuddyBlock* buddy_blk = (SSBuddyBlock*)((uint8_t*)blk + order_size(order));
        buddy_blk->order = order;
        int idx = order_to_index(order);
        buddy_blk->next = buddy.free_lists[idx];
        buddy.free_lists[idx] = buddy_blk;
        buddy.order_map[block_index(buddy_blk)] = order;
    }
    blk->order = to_order;
    buddy.order_map[block_index(blk)] = to_order;
}

void* ss_alloc(uint32_t size) {
    if (size == 0) return NULL;

    /* Add block header overhead */
    size += sizeof(SSBuddyBlock);
    /* Round up to next power of 2 */
    uint8_t order = SS_BUDDY_MIN_ORDER;
    while (order_size(order) < size && order <= SS_BUDDY_MAX_ORDER) {
        order++;
    }
    if (order > SS_BUDDY_MAX_ORDER) return NULL;

    /* Find a free block of sufficient order */
    uint8_t found_order = order;
    while (found_order <= SS_BUDDY_MAX_ORDER) {
        int fidx = order_to_index(found_order);
        if (buddy.free_lists[fidx] != NULL) break;
        found_order++;
    }
    if (found_order > SS_BUDDY_MAX_ORDER) return NULL;

    /* Remove block from free list */
    int fidx = order_to_index(found_order);
    SSBuddyBlock* blk = buddy.free_lists[fidx];
    buddy.free_lists[fidx] = blk->next;

    /* Split if necessary */
    if (found_order > order) {
        split_block(blk, found_order, order);
    }

    /* Mark as allocated in order map */
    buddy.order_map[block_index(blk)] = 0xFF;

    /* Return pointer past the block header */
    return (void*)((uint8_t*)blk + sizeof(SSBuddyBlock));
}

void ss_free(void* ptr) {
    if (ptr == NULL) return;

    SSBuddyBlock* blk = (SSBuddyBlock*)((uint8_t*)ptr - sizeof(SSBuddyBlock));
    uint8_t order = blk->order;
    int oidx = order_to_index(order);

    /* Try to coalesce with buddy */
    while (order < SS_BUDDY_MAX_ORDER) {
        SSBuddyBlock* buddy_blk = (SSBuddyBlock*)block_buddy(blk, order);

        /* Check if buddy is free and same order */
        uint32_t buddy_idx = block_index(buddy_blk);
        if (buddy_idx >= buddy.map_entries) break;

        uint8_t buddy_order = buddy.order_map[buddy_idx];
        if (buddy_order != order) break;

        /* Remove buddy from its free list */
        SSBuddyBlock** pp = &buddy.free_lists[oidx];
        while (*pp != NULL && *pp != buddy_blk) {
            pp = &(*pp)->next;
        }
        if (*pp == NULL) break;
        *pp = buddy_blk->next;

        /* Merge: use lower address as merged block */
        if (buddy_blk < blk) blk = buddy_blk;
        order++;
        oidx = order_to_index(order);
        blk->order = order;
        buddy.order_map[block_index(blk)] = order;
    }

    /* Add to free list */
    blk->next = buddy.free_lists[oidx];
    buddy.free_lists[oidx] = blk;
    buddy.order_map[block_index(blk)] = order;
}

void* ss_alloc_aligned(uint32_t size, uint32_t align) {
    if (align < sizeof(SSBuddyBlock)) align = sizeof(SSBuddyBlock);
    if (align & (align - 1)) return NULL;

    uint32_t extra = align - 1 + sizeof(void*);
    void* raw = ss_alloc(size + extra);
    if (!raw) return NULL;

    uintptr_t addr = (uintptr_t)raw + sizeof(void*);
    uintptr_t aligned = (addr + align - 1) & ~(uintptr_t)(align - 1);

    ((void**)aligned)[-1] = raw;
    return (void*)aligned;
}

void ss_free_aligned(void* ptr) {
    if (!ptr) return;
    void* raw = ((void**)ptr)[-1];
    ss_free(raw);
}

uint32_t ss_mem_total(void) {
    return buddy.total_size;
}

uint32_t ss_mem_free_bytes(void) {
    uint32_t total = 0;
    for (int i = 0; i < SS_BUDDY_ORDERS; i++) {
        SSBuddyBlock* blk = buddy.free_lists[i];
        while (blk) {
            total += order_size(blk->order);
            blk = blk->next;
        }
    }
    return total;
}
