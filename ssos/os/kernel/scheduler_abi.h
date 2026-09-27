#ifndef SS_SCHEDULER_ABI_H
#define SS_SCHEDULER_ABI_H

/* Fields read directly by the context-switch assembly. Keep this as the
 * single layout definition for C assertions and both production/QEMU ports. */
#define SS_TCB_CONTEXT_OFFSET_VALUE 0
#define SS_TCB_STACK_BASE_OFFSET_VALUE 12
#define SS_TCB_ENTRY_OFFSET_VALUE 20
#define SS_TCB_RESUME_TYPE_OFFSET_VALUE 31
#define SS_TCB_SLEEP_NEXT_OFFSET_VALUE 32
#define SS_TCB_ARG_OFFSET_VALUE 38
#define SS_TCB_SIZE_VALUE 42

#ifdef __ASSEMBLER__
#define SS_TCB_CONTEXT_OFFSET(reg) SS_TCB_CONTEXT_OFFSET_VALUE(reg)
#define SS_TCB_STACK_BASE_OFFSET(reg) SS_TCB_STACK_BASE_OFFSET_VALUE(reg)
#define SS_TCB_ENTRY_OFFSET(reg) SS_TCB_ENTRY_OFFSET_VALUE(reg)
#define SS_TCB_RESUME_TYPE_OFFSET(reg) SS_TCB_RESUME_TYPE_OFFSET_VALUE(reg)
#define SS_TCB_ARG_OFFSET(reg) SS_TCB_ARG_OFFSET_VALUE(reg)
#endif

#endif
