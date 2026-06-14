#include "memory.h"
#include <stdint.h>
#include <string.h>

void ss_slab_init(SSSlabCache* cache, uint16_t obj_size, void* mem, uint32_t mem_size) {
    cache->obj_size = obj_size;
    cache->count = mem_size / obj_size;
    cache->free_count = cache->count;
    cache->pad = 0;

    /* Build free list */
    SSSlabObj* obj = (SSSlabObj*)mem;
    cache->free_list = obj;
    for (uint16_t i = 0; i < cache->count - 1; i++) {
        SSSlabObj* next = (SSSlabObj*)((uint8_t*)obj + obj_size);
        obj->next = next;
        obj = next;
    }
    obj->next = NULL;
}

void* ss_slab_alloc(SSSlabCache* cache) {
    if (cache->free_list == NULL) return NULL;
    SSSlabObj* obj = cache->free_list;
    cache->free_list = obj->next;
    cache->free_count--;
    return (void*)obj;
}

void ss_slab_free(SSSlabCache* cache, void* obj) {
    if (obj == NULL) return;
    SSSlabObj* slab_obj = (SSSlabObj*)obj;
    slab_obj->next = cache->free_list;
    cache->free_list = slab_obj;
    cache->free_count++;
}
