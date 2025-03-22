#include "memory.h"
#include "kernel.h"
#include <stdint.h>

void ss_get_ssos_memory(void** base, uint32_t* sz) {
#ifdef LOCAL_MODE
    *base = local_ssos_memory_base;
    *sz = local_ssos_memory_size;
#else
    *base = (void*)__ssosram_start;
    *sz = (uint32_t)__ssosram_size;
#endif
}

void ss_get_app_memory(void** base, uint32_t* sz) {
#ifdef LOCAL_MODE
    *base = local_app_memory_base;
    *sz = local_app_memory_size;
#else
    *base = (void*)__appram_start;
    *sz = (uint32_t)__appram_size;
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
