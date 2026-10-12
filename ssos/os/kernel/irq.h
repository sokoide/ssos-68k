#ifndef SS_IRQ_H
#define SS_IRQ_H

#include <stdint.h>

/* Supervisor-only. Each nesting level restores its own saved SR. */
uint16_t ss_irq_save(void);
void ss_irq_restore(uint16_t saved_sr);

#endif /* SS_IRQ_H */
