#ifndef S3_MEMORY_H
#define S3_MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* Buddy system constants */
#define S3_BUDDY_MIN_ORDER  4    /* 2^4 = 16 bytes minimum */
#define S3_BUDDY_MAX_ORDER  16   /* 2^16 = 64KB maximum */
#define S3_BUDDY_ORDERS     (S3_BUDDY_MAX_ORDER - S3_BUDDY_MIN_ORDER + 1)

/* Slab cache constants */
#define S3_SLAB_SIZE        (64 * 1024)

typedef struct S3BuddyBlock S3BuddyBlock;
struct S3BuddyBlock {
    S3BuddyBlock* next;
    uint8_t order;
    uint8_t pad[3];
};

typedef struct {
    S3BuddyBlock* free_lists[S3_BUDDY_ORDERS];
    uint8_t* order_map;         /* Map: block -> order */
    void*    base;
    uint32_t total_size;
    uint32_t map_entries;
} S3BuddySystem;

/* Slab cache */
typedef struct S3SlabObj S3SlabObj;
struct S3SlabObj {
    S3SlabObj* next;
};

typedef struct {
    S3SlabObj* free_list;
    uint16_t obj_size;
    uint16_t count;
    uint16_t free_count;
    uint16_t pad;
} S3SlabCache;

/* Pre-defined slab caches */
extern S3SlabCache s3_slab_task;
extern S3SlabCache s3_slab_window;
extern S3SlabCache s3_slab_msg;
extern S3SlabCache s3_slab_rect;

void     s3_mem_init(void* base, uint32_t size);
void*    s3_alloc(uint32_t size);
void     s3_free(void* ptr);
void*    s3_alloc_aligned(uint32_t size, uint32_t align);

void     s3_slab_init(S3SlabCache* cache, uint16_t obj_size, void* mem, uint32_t mem_size);
void*    s3_slab_alloc(S3SlabCache* cache);
void     s3_slab_free(S3SlabCache* cache, void* obj);

uint32_t s3_mem_total(void);
uint32_t s3_mem_free_bytes(void);

#endif /* S3_MEMORY_H */
