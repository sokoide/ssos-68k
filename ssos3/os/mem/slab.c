#include "memory.h"
#include <stdint.h>
#include <string.h>

void s3_slab_init(S3SlabCache* cache, uint16_t obj_size, void* mem, uint32_t mem_size) {
    cache->obj_size = obj_size;
    cache->count = mem_size / obj_size;
    cache->free_count = cache->count;
    cache->pad = 0;

    /* Build free list */
    S3SlabObj* obj = (S3SlabObj*)mem;
    cache->free_list = obj;
    for (uint16_t i = 0; i < cache->count - 1; i++) {
        S3SlabObj* next = (S3SlabObj*)((uint8_t*)obj + obj_size);
        obj->next = next;
        obj = next;
    }
    obj->next = NULL;
}

void* s3_slab_alloc(S3SlabCache* cache) {
    if (cache->free_list == NULL) return NULL;
    S3SlabObj* obj = cache->free_list;
    cache->free_list = obj->next;
    cache->free_count--;
    return (void*)obj;
}

void s3_slab_free(S3SlabCache* cache, void* obj) {
    if (obj == NULL) return;
    S3SlabObj* slab_obj = (S3SlabObj*)obj;
    slab_obj->next = cache->free_list;
    cache->free_list = slab_obj;
    cache->free_count++;
}
