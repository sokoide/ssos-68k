#ifndef SS_BOOT_H
#define SS_BOOT_H

#include <stdint.h>

/* Entry points */
void premain(void);
int16_t ss_init(void);
void ss_run(void);

/* Linker symbols */
extern uint8_t __text_start, __text_end, __text_size;
extern uint8_t __data_start, __data_end, __data_size;
extern uint8_t __bss_start, __bss_end, __bss_size;
extern uint8_t __ssosram_start, __ssosram_size;
extern uint8_t __ssosstack_start, __ssosstack_end, __ssosstack_size;

#endif /* SS_BOOT_H */
