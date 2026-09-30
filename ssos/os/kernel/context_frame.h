#ifndef SS_CONTEXT_FRAME_H
#define SS_CONTEXT_FRAME_H
#define SS_FRAME_REG_WORDS 15
#define SS_FRAME_REG_BYTES 60
#define SS_FRAME_ARG_BYTES 4
#define SS_FRAME_EXCEPTION_SR 60
#define SS_FRAME_EXCEPTION_PC 62
#define SS_FRAME_COOP_SR 60
#define SS_FRAME_COOP_PC 62
#define SS_FRAME_PRE_PC 60
#define SS_FRAME_PRE_SR 64
#define SS_FRAME_BYTES 66
#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>
typedef struct __attribute__((packed, aligned(2))) {
    uint32_t regs[SS_FRAME_REG_WORDS];
    uint16_t sr;
    uint32_t pc;
} SSCoopFrame;
typedef struct __attribute__((packed, aligned(2))) {
    uint32_t regs[SS_FRAME_REG_WORDS];
    uint32_t pc;
    uint16_t sr;
} SSPreYieldFrame;
_Static_assert(offsetof(SSCoopFrame, sr) == SS_FRAME_COOP_SR, "coop SR frame");
_Static_assert(offsetof(SSCoopFrame, pc) == SS_FRAME_COOP_PC, "coop PC frame");
_Static_assert(offsetof(SSPreYieldFrame, sr) == SS_FRAME_PRE_SR, "pre SR frame");
_Static_assert(offsetof(SSPreYieldFrame, pc) == SS_FRAME_PRE_PC, "pre PC frame");
_Static_assert(sizeof(SSCoopFrame) == SS_FRAME_BYTES, "coop frame size");
_Static_assert(sizeof(SSPreYieldFrame) == SS_FRAME_BYTES, "pre frame size");
#endif
#endif
