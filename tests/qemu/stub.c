/* stub.c - QEMU-side HW stubs + freestanding runtime for the scheduler test.
 *
 * The cooperative scheduler.c compiles unmodified for m68k-elf-gcc; this file
 * supplies everything it references that isn't the CPU itself:
 *   - the tick/vsync counters (normally bumped by the Timer D / V-DISP ISRs)
 *   - the task stack arena (normally provided by app/main.c)
 *   - memset/memcpy (we build -nostdlib)
 *   - a Goldfish-TTY printer (for test output)
 *
 * The actual context switch lives in ctx_switch.s. */

#include <stdint.h>
#include <stddef.h>

#define SS_TASK_STACK 16384
#define SS_MAX_TASKS  32

/* ---- counters (defined in interrupts.s on real HW) ------------------- */
volatile uint32_t ss_tick_counter     = 0;
volatile uint32_t ss_vsync_counter    = 0;
volatile uint32_t ss_vdisp_fire_count = 0;
volatile uint32_t ss_timerd_fire_count = 0;
volatile uint8_t  ss_wakeups_needed   = 0;

/* ---- task stack arena (real: app/main.c provides this) --------------- */
static uint8_t stack_arena[SS_MAX_TASKS * SS_TASK_STACK] __attribute__((aligned(4)));
uint8_t* ss_task_stack_base = stack_arena;

/* ---- freestanding string helpers (-nostdlib) ------------------------- */
void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
    return dst;
}

/* ---- Goldfish TTY output (QEMU virt) --------------------------------- */
#define GOLDFISH_TTY  ((volatile uint32_t*)0xff008000)

void tty_putc(char c) {
    *GOLDFISH_TTY = (uint32_t)(uint8_t)c;
}

void tty_puts(const char* s) {
    while (*s) tty_putc(*s++);
}

/* Tiny decimal printer for a small uint (test counter). */
void tty_putu(unsigned v) {
    char buf[8];
    int i = (int)sizeof(buf);
    buf[--i] = '\0';
    if (v == 0) {
        buf[--i] = '0';
    } else {
        while (v) { buf[--i] = (char)('0' + (v % 10)); v /= 10; }
    }
    tty_puts(&buf[i]);
}
