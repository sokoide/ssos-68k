/* tty.h - Goldfish TTY output helpers shared by the QEMU scheduler tests.
 * Implemented in common/stub.c. */
#ifndef SS_QEMU_TTY_H
#define SS_QEMU_TTY_H

void tty_putc(char c);
void tty_puts(const char* s);
void tty_putu(unsigned v);

#endif
