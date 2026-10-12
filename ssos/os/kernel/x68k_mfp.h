#ifndef SS_X68K_MFP_H
#define SS_X68K_MFP_H

#include <stdint.h>

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

/* X68000 MFP setup and saved-state restoration. */
void ss_set_interrupts(void);
void ss_restore_interrupts(void);

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

#endif /* SS_X68K_MFP_H */
