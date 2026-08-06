#include "../scheduler.h"

extern volatile uint8_t ss_wakeups_needed;

void ss_process_wakeups(void) {
    if (ss_wakeups_needed) {
        ss_wakeups_needed = 0;
        ss_do_wakeups();
    }
}
