#ifndef S3_KERNEL_H
#define S3_KERNEL_H

#include <stddef.h>
#include <stdint.h>

/* Error codes */
#define S3_OK          0
#define S3_ERR        (-1)
#define S3_ERR_PARAM  (-2)
#define S3_ERR_MEMORY (-3)
#define S3_ERR_LIMIT  (-4)
#define S3_ERR_STATE  (-5)
#define S3_ERR_ID     (-6)

/* Hardware addresses - MFP */
#define S3_MFP_BASE     0xE88000
#define S3_DMAC_BASE    0xE84000
#define S3_DMA_CH2_BASE 0xE84080

/* Interrupt vector addresses */
#define S3_VEC_TIMERA  0x134
#define S3_VEC_TIMERB  0x120
#define S3_VEC_TIMERC  0x114
#define S3_VEC_TIMERD  0x110
#define S3_VEC_KEY     0x130
#define S3_VEC_VDISP   0x134
#define S3_VEC_CRTC    0x138
#define S3_VEC_HSYNC   0x13C

/* Task constants */
#define S3_MAX_TASKS   32
#define S3_MAX_PRI     16
#define S3_TASK_STACK  16384

/* Context save levels */
#define S3_CTX_MINIMAL 0x00
#define S3_CTX_NORMAL  0x01
#define S3_CTX_FULL    0x02

/* Task states */
#define S3_TS_NONE     0
#define S3_TS_DORMANT  1
#define S3_TS_READY    2
#define S3_TS_WAIT     3

/* Global counters (defined in interrupts.s) */
extern volatile uint32_t s3_tick_counter;
extern volatile uint32_t s3_vsync_counter;

/* Entry points */
void premain(void);
void s3_init(void);
void s3_run(void);

/* Interrupt setup (defined in interrupts.s) */
void s3_set_interrupts(void);
void s3_restore_interrupts(void);
void s3_disable_interrupts(void);
void s3_enable_interrupts(void);

/* Linker symbols */
extern uint8_t __text_start, __text_end, __text_size;
extern uint8_t __data_start, __data_end, __data_size;
extern uint8_t __bss_start, __bss_end, __bss_size;
extern uint8_t __ssosram_start, __ssosram_size;

#endif /* S3_KERNEL_H */
