#include "memory.h"
#include "kernel.h"
#include <stdint.h>

void* ss_ssos_memory_base;
uint32_t ss_ssos_memory_size;
SsMemMgr ss_mem_mgr;

void ss_init_memory_info() {
    ss_get_ssos_memory(&ss_ssos_memory_base, &ss_ssos_memory_size);
}

void ss_get_ssos_memory(void** base, uint32_t* sz) {
#ifdef LOCAL_MODE
    *base = local_ssos_memory_base;
    *sz = local_ssos_memory_size;
#else
    *base = (void*)__ssosram_start;
    *sz = (uint32_t)__ssosram_size;
#endif
}

void ss_get_text(void** base, uint32_t* sz) {
#ifdef LOCAL_MODE
    *base = (void*)0;
    *sz = local_text_size;
#else
    *base = (void*)__text_start;
    *sz = (uint32_t)__text_size;
#endif
}

void ss_get_data(void** base, uint32_t* sz) {
#ifdef LOCAL_MODE
    *base = (void*)0;
    *sz = local_data_size;
#else
    *base = (void*)__data_start;
    *sz = (uint32_t)__data_size;
#endif
}

void ss_get_bss(void** base, uint32_t* sz) {
#ifdef LOCAL_MODE
    *base = (void*)0;
    *sz = local_bss_size;
#else
    *base = (void*)__bss_start;
    *sz = (uint32_t)__bss_size;
#endif
}

void ss_mem_init() {
    ss_mem_mgr.num_free_blocks = 0;
    ss_mem_free((uint32_t)ss_ssos_memory_base, ss_ssos_memory_size);
}

int ss_mem_free(uint32_t addr, uint32_t sz) {
    int i;
    for (i = 0; i < ss_mem_mgr.num_free_blocks; i++) {
        if (ss_mem_mgr.free_blocks[i].addr > addr)
            break;
    }
    if (i > 0) {
        // check if the block to be freed can be combined with the previous
        // block
        if (ss_mem_mgr.free_blocks[i - 1].addr +
                ss_mem_mgr.free_blocks[i - 1].sz ==
            addr) {
            // can combine
            ss_mem_mgr.free_blocks[i - 1].sz += sz;
            // check the next free block
            if (i < ss_mem_mgr.num_free_blocks &&
                ss_mem_mgr.free_blocks[i].addr == addr + sz) {
                // combine it with the next block
                ss_mem_mgr.free_blocks[i - 1].sz +=
                    ss_mem_mgr.free_blocks[i].sz;
                ss_mem_mgr.num_free_blocks--;
                for (; i < ss_mem_mgr.num_free_blocks; i++) {
                    ss_mem_mgr.free_blocks[i] = ss_mem_mgr.free_blocks[i + 1];
                }
                return 0;
            }
        }
    }
    if (i < ss_mem_mgr.num_free_blocks) {
        // check the next block
        if (ss_mem_mgr.free_blocks[i].sz == addr + sz) {
            // can combine
            ss_mem_mgr.free_blocks[i].addr = addr;
            ss_mem_mgr.free_blocks[i].sz += sz;
            return 0;
        }
    }
    // can't combine
    if (ss_mem_mgr.num_free_blocks < MEM_FREE_BLOCKS) {
        for (int j = ss_mem_mgr.num_free_blocks; j > i; j--) {
            ss_mem_mgr.free_blocks[j] = ss_mem_mgr.free_blocks[j - 1];
        }
        ss_mem_mgr.free_blocks[i].addr = addr;
        ss_mem_mgr.free_blocks[i].sz = sz;
        ss_mem_mgr.num_free_blocks++;
        return 0;
    }

    // no space
    return -1;
}

int ss_mem_free4k(uint32_t addr, uint32_t sz) {
    sz = (sz + 0xfff) & 0xfffff000;
    return ss_mem_free(addr, sz);
}

uint32_t ss_mem_alloc(uint32_t sz) {
    int i;
    uint32_t addr;
    for (i = 0; i < ss_mem_mgr.num_free_blocks; i++) {
        if (ss_mem_mgr.free_blocks[i].sz >= sz) {
            addr = ss_mem_mgr.free_blocks[i].addr;
            ss_mem_mgr.free_blocks[i].addr += sz;
            ss_mem_mgr.free_blocks[i].sz -= sz;
            if (ss_mem_mgr.free_blocks[i].sz == 0) {
                // remove this free block
                ss_mem_mgr.num_free_blocks--;
                for (; i < ss_mem_mgr.num_free_blocks; i++) {
                    ss_mem_mgr.free_blocks[i] = ss_mem_mgr.free_blocks[i + 1];
                }
            }
            return addr;
        }
    }

    return 0;
}

uint32_t ss_mem_alloc4k(uint32_t sz) {
    sz = (sz + 0xfff) & 0xfffff000;
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
