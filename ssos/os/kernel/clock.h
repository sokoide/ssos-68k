#ifndef SS_CLOCK_H
#define SS_CLOCK_H

#include <stdint.h>

/* Global counters (defined in interrupts.s) */
extern volatile uint32_t ss_tick_counter;
extern volatile uint32_t ss_vsync_counter;
extern volatile uint32_t ss_vdisp_fire_count;
extern volatile uint32_t ss_timerd_fire_count;

#endif /* SS_CLOCK_H */
