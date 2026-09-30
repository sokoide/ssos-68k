#include "scheduler.h"
#include "tty.h"
extern void ss_restore_trap14(void);
extern uint32_t ss_old_trap14;
void ss_abort_cleanup(void) {} /* Not exercised: only production restore entry. */
static uint16_t read_sr(void) {
    uint16_t sr;
    __asm__ volatile("move.w %%sr,%0" : "=d"(sr));
    return sr;
}
int main(void) {
    ss_old_trap14 = *(volatile uint32_t*)0xB8;
    uint16_t original = read_sr();
    for (unsigned i = 0; i < 8; i++) {
        uint16_t before = 0x2000u | (i << 8);
        ss_irq_restore(before);
        ss_restore_trap14();
        if ((read_sr() & 0x2700) != before) {
            tty_puts("FAIL production trap restore SR\n"); for (;;) {}
        }
        uint16_t outer = ss_irq_save();
        ss_restore_trap14();
        if ((read_sr() & 0x2700) != 0x2700) {
            tty_puts("FAIL nested production trap restore\n"); for (;;) {}
        }
        ss_irq_restore(outer);
    }
    ss_irq_restore(original);
    tty_puts("OK production trap restore IPL 0-7 + nesting\n");
    for (;;) {}
}
