#ifndef SS_MEMORY_H
#define SS_MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* Buddy system constants */
#define SS_BUDDY_MIN_ORDER  4    /* 2^4 = 16 bytes minimum */
#define SS_BUDDY_MAX_ORDER  16   /* 2^16 = 64KB maximum */
#define SS_BUDDY_ORDERS     (SS_BUDDY_MAX_ORDER - SS_BUDDY_MIN_ORDER + 1)

/* Slab cache constants */
#define SS_SLAB_SIZE        (64 * 1024)

typedef struct SSBuddyBlock SSBuddyBlock;
struct SSBuddyBlock {
    SSBuddyBlock* next;
    uint8_t order;
    uint8_t pad[3];
};

typedef struct {
    SSBuddyBlock* free_lists[SS_BUDDY_ORDERS];
    uint8_t* order_map;         /* Map: block -> order */
    void*    base;
    uint32_t total_size;
    uint32_t map_entries;
} SSBuddySystem;

/* Slab cache */
typedef struct SSSlabObj SSSlabObj;
struct SSSlabObj {
    SSSlabObj* next;
};

typedef struct {
    SSSlabObj* free_list;
    uint16_t obj_size;
    uint16_t count;
    uint16_t free_count;
    uint16_t pad;
} SSSlabCache;

/* Pre-defined slab caches */
extern SSSlabCache ss_slab_task;
extern SSSlabCache ss_slab_window;
extern SSSlabCache ss_slab_msg;
extern SSSlabCache ss_slab_rect;

void     ss_mem_init(void* base, uint32_t size);
void*    ss_alloc(uint32_t size);
void     ss_free(void* ptr);
void*    ss_alloc_aligned(uint32_t size, uint32_t align);

void     ss_slab_init(SSSlabCache* cache, uint16_t obj_size, void* mem, uint32_t mem_size);
void*    ss_slab_alloc(SSSlabCache* cache);
void     ss_slab_free(SSSlabCache* cache, void* obj);

uint32_t ss_mem_total(void);
uint32_t ss_mem_free_bytes(void);

#endif /* SS_MEMORY_H */
