#ifndef SS_KERNEL_H
#define SS_KERNEL_H

#include <stddef.h>
#include <stdint.h>

/* Error codes */
#define SS_OK          0
#define SS_ERR        (-1)
#define SS_ERR_PARAM  (-2)
#define SS_ERR_MEMORY (-3)
#define SS_ERR_LIMIT  (-4)
#define SS_ERR_STATE  (-5)
#define SS_ERR_ID     (-6)

/* Hardware addresses - MFP */
#define SS_MFP_BASE     0xE88000
#define SS_DMAC_BASE    0xE84000
#define SS_DMA_CH2_BASE 0xE84080

/* Interrupt vector addresses */
#define SS_VEC_TIMERA  0x134
#define SS_VEC_TIMERB  0x120
#define SS_VEC_TIMERC  0x114
#define SS_VEC_TIMERD  0x110
#define SS_VEC_KEY     0x130
#define SS_VEC_VDISP   0x134
#define SS_VEC_CRTC    0x138
#define SS_VEC_HSYNC   0x13C

/* Task constants */
#define SS_MAX_TASKS   32
#define SS_MAX_PRI     16
#define SS_TASK_STACK  16384

/* Context save levels */
#define SS_CTX_MINIMAL 0x00
#define SS_CTX_NORMAL  0x01
#define SS_CTX_FULL    0x02

/* Task states */
#define SS_TS_NONE     0
#define SS_TS_DORMANT  1
#define SS_TS_READY    2
#define SS_TS_WAIT     3

/* Global counters (defined in interrupts.s) */
extern volatile uint32_t ss_tick_counter;
extern volatile uint32_t ss_vsync_counter;
extern volatile uint32_t ss_vdisp_fire_count;
extern volatile uint32_t ss_timerd_fire_count;

/* Entry points */
void premain(void);
void ss_init(void);
void ss_run(void);

/* Interrupt setup (defined in interrupts.s) */
void ss_set_interrupts(void);
void ss_restore_interrupts(void);
void ss_disable_interrupts(void);
void ss_enable_interrupts(void);

/* Linker symbols */
extern uint8_t __text_start, __text_end, __text_size;
extern uint8_t __data_start, __data_end, __data_size;
extern uint8_t __bss_start, __bss_end, __bss_size;
extern uint8_t __ssosram_start, __ssosram_size;

/* MFP register addresses */
#define SS_MFP_IERA  (*(volatile uint8_t*)0xE88007)
#define SS_MFP_IERB  (*(volatile uint8_t*)0xE88009)
#define SS_MFP_IPRA  (*(volatile uint8_t*)0xE8800B)
#define SS_MFP_IPRB  (*(volatile uint8_t*)0xE8800D)
#define SS_MFP_ISRA  (*(volatile uint8_t*)0xE8800F)
#define SS_MFP_ISRB  (*(volatile uint8_t*)0xE88011)
#define SS_MFP_IMRA  (*(volatile uint8_t*)0xE88013)
#define SS_MFP_IMRB  (*(volatile uint8_t*)0xE88015)
#define SS_MFP_VR    (*(volatile uint8_t*)0xE88017)
#define SS_MFP_TACR  (*(volatile uint8_t*)0xE88019)
#define SS_MFP_TCDCR (*(volatile uint8_t*)0xE8801D)
#define SS_MFP_TADR  (*(volatile uint8_t*)0xE8801F)
#define SS_MFP_TBDR  (*(volatile uint8_t*)0xE88021)
#define SS_MFP_TCDR  (*(volatile uint8_t*)0xE88023)
#define SS_MFP_TDDR  (*(volatile uint8_t*)0xE88025)

/* MFP register snapshot for debugging */
typedef struct {
    uint8_t iera;
    uint8_t ierb;
    uint8_t ipra;
    uint8_t iprb;
    uint8_t isra;
    uint8_t isrb;
    uint8_t imra;
    uint8_t imrb;
    uint8_t vr;
    uint8_t tacr;
    uint8_t tcdcr;
    uint8_t tadr;
    uint8_t tbdr;
    uint8_t tcdr;
    uint8_t tddr;
} SSMfpRegs;

/* MFP debug functions */
void ss_dump_mfp_regs(SSMfpRegs* out);
int  ss_diff_mfp_regs(const SSMfpRegs* before, const SSMfpRegs* after,
                      char* buf, int bufsize);
void ss_mfp_track_begin(SSMfpRegs* snapshot);
void ss_mfp_track_end(const SSMfpRegs* before, const char* label);
void ss_mfp_watch_ctrl(const char* label);

#endif /* SS_KERNEL_H */
