# QEMU cooperative-scheduler test.
#
# Builds the REAL ssos/os/kernel/cooperative/scheduler.c (unmodified) plus a
# QEMU port of the context switch (ctx_switch.s) and runs it under
# qemu-system-m68k -M virt. Two worker tasks round-robin with main via real
# register save/restore (movem.l) — the scenario the Native stub can't test.

CC      = m68k-elf-gcc
ASFLAGS = -m68000 -Wa,--register-prefix-optional
CFLAGS  = -m68000 -O2 -g -ffreestanding -nostdlib -fno-builtin \
          -Wall -Wextra -Wno-unused-parameter \
          -Iinclude -I../../ssos/os/kernel
LDFLAGS = -T linker.ld -nostdlib -Wl,--gc-sections -lgcc

QEMU    = qemu-system-m68k
QFLAGS  = -M virt -cpu m68000 -nographic
TIMEOUT_SECS ?= 2

SSOS    = ../../ssos/os

OBJS = main_coop.o stub.o ctx_switch.o sched.o
TARGET = coop_sched.elf

.PHONY: all run clean

all: $(TARGET)

# scheduler.c is shared source; compile it freestanding for m68k.
sched.o: $(SSOS)/kernel/cooperative/scheduler.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(ASFLAGS) -c $< -o $@

$(TARGET): $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $(TARGET)

run: $(TARGET)
	timeout --foreground $(TIMEOUT_SECS) $(QEMU) $(QFLAGS) -kernel $(TARGET) || true

clean:
	rm -f $(OBJS) $(TARGET)
