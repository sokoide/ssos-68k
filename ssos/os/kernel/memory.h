#pragma once
#include <stdint.h>

/* common)
 * 0x000000-0x001FFF: Interrupt vector, IOCS work or etc (8KiB)
 *
 * disk boot mode)
 * 0x002000-0x0023FF: Boot sector (1KiB)
 * 0x0023FF-0x00FFFF: SSOS supervisor mode stack (55KiB)
 * 0x010000-0x02FFFF: SSOS .text (128KiB)
 * 0x030000-0x03FFFF: SSOS .data (64KiB)
 * 0x150000-0x15FFFF: SSOS .ssos (64KiB) - not used yet
 * 0x160000-0xBFFFFF: SSOS .app (10.7MiB), application memory
 *
 * local mode)
 * supervisor stack, .text, .data, .bss auto assigned
 * app memory allocated by malloc()
 *
 * 1 memory chunk = 4KiB
 * 11MiB / 4Kib = 2816 chunks
 */

extern void* ss_ssos_memory_base;
extern void* ss_app_memory_base;
extern uint32_t ss_ssos_memory_size;
extern uint32_t ss_app_memory_size;

void ss_init_memory_info();
void ss_get_ssos_memory(void** base, uint32_t* sz);
void ss_get_app_memory(void** base, uint32_t* sz);
void ss_get_text(void** base, uint32_t* sz);
void ss_get_data(void** base, uint32_t* sz);
void ss_get_bss(void** base, uint32_t* sz);

#define MEM_FREE_BLOCKS 1024

typedef struct ss_mem_free_block {
    uint32_t addr;
    uint32_t sz;
} SsMemFreeBlock;

typedef struct ss_mem_mamager {
    int num_free_blocks;
    struct ss_mem_free_block free_blocks[MEM_FREE_BLOCKS];
} SsMemManager;

void ss_mem_init();
int ss_mem_free(uint32_t addr, uint32_t sz);
int ss_mem_free4k(uint32_t addr, uint32_t sz);
uint32_t ss_mem_alloc(uint32_t sz);
uint32_t ss_mem_alloc4k(uint32_t sz);
uint32_t ss_mem_total_bytes();
uint32_t ss_mem_free_bytes();

// temp
extern SsMemManager ss_mem_mgr;
