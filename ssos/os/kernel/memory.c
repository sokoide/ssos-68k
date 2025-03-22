#include "memory.h"
#include "kernel.h"
#include <stdint.h>

void* ss_get_app_memory_base() {
#ifdef LOCAL_MODE
    return local_app_memory_base;
#else
    return (void*)__appram_start;
#endif
}

uint32_t ss_get_app_memory_size() {
#ifdef LOCAL_MODE
    return local_app_memory_size;
#else
    return (uint32_t)__appram_size;
#endif
}
