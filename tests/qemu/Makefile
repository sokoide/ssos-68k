# QEMU scheduler tests.
#
# Two flavors, each driving the REAL SSOS scheduler.c (unmodified) on a real
# m68k under qemu-system-m68k -M virt:
#
#   coop    (make run)         - cooperative: tasks yield via movem.l/jmp
#   preempt (make run-preempt) - preemptive:  trap #0 -> Timer D ISR -> rte
#
# The cooperative build compiles cooperative/scheduler.c + ctx_switch.s.
# The preemptive build compiles preemptive/scheduler.c + preempt_ctx_switch.s.
# `make all` builds both; `make runall` runs both.

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

# ---- cooperative ----
COOP_OBJS = main_coop.o stub.o ctx_switch.o sched_coop.o
COAP_ELF = coop_sched.elf

# ---- preemptive ----
PRE_OBJS = main_preempt.o stub.o preempt_ctx_switch.o sched_pre.o
PRE_ELF = preempt_sched.elf

.PHONY: all run run-preempt runall clean

all: $(COAP_ELF) $(PRE_ELF)

# scheduler.c differs per variant; build to distinct object names to avoid
# stale-mixing when switching.
sched_coop.o: $(SSOS)/kernel/cooperative/scheduler.c
	$(CC) $(CFLAGS) -c $< -o $@

sched_pre.o: $(SSOS)/kernel/preemptive/scheduler.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(ASFLAGS) -c $< -o $@

$(COAP_ELF): $(COOP_OBJS) linker.ld
	$(CC) $(CFLAGS) $(COOP_OBJS) $(LDFLAGS) -o $(COAP_ELF)

$(PRE_ELF): $(PRE_OBJS) linker.ld
	$(CC) $(CFLAGS) $(PRE_OBJS) $(LDFLAGS) -o $(PRE_ELF)

run: $(COAP_ELF)
	timeout --foreground $(TIMEOUT_SECS) $(QEMU) $(QFLAGS) -kernel $(COAP_ELF) || true

run-preempt: $(PRE_ELF)
	timeout --foreground $(TIMEOUT_SECS) $(QEMU) $(QFLAGS) -kernel $(PRE_ELF) || true

runall: run run-preempt

clean:
	rm -f *.o $(COAP_ELF) $(PRE_ELF)
