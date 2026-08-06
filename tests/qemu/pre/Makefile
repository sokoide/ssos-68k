# Preemptive QEMU scheduler tests.
# Builds the real shared scheduler and preemptive wakeup policy plus a
# QEMU port of the Timer D ISR path (preempt_ctx_switch.s) and runs each test
# under qemu-system-m68k -M virt. Tests fire `trap #0` to drive preemption via
# the `.resume_interrupted`/`rte` path.

CC      = m68k-elf-gcc
ASFLAGS = -m68000 -Wa,--register-prefix-optional
CFLAGS  = -m68000 -O2 -g -ffreestanding -nostdlib -fno-builtin \
          -Wall -Wextra -Wno-unused-parameter \
          -I../common -I../common/include -I../../../ssos/os/kernel
LDFLAGS = -T ../common/linker.ld -nostdlib -Wl,--gc-sections -lgcc

QEMU    = qemu-system-m68k
QFLAGS  = -M virt -cpu m68000 -nographic
TIMEOUT_SECS ?= 2

SSOS   = ../../../ssos/os

# t04_main_task_register exercises the production main-task bootstrap.
# t05_timerd_cadence rebuilds the ISR port with the production 10-tick cadence.
TESTS = t01_round_robin t02_register_save t03_sleep_wakeup t04_main_task_register \
	t05_timerd_cadence

# Shared objects
stub.o: ../common/stub.c
	$(CC) $(CFLAGS) -c $< -o $@
sched.o: $(SSOS)/kernel/scheduler.c
	$(CC) $(CFLAGS) -c $< -o $@
wakeups.o: $(SSOS)/kernel/preemptive/wakeups.c
	$(CC) $(CFLAGS) -c $< -o $@
main_task.o: $(SSOS)/kernel/main_task.c
	$(CC) $(CFLAGS) -c $< -o $@
preempt_ctx_switch.o: preempt_ctx_switch.s
	$(CC) $(ASFLAGS) -c $< -o $@
preempt_ctx_switch_10.o: preempt_ctx_switch.s
	$(CC) $(ASFLAGS) -Wa,--defsym,SS_TIMERD_SWITCH_TICKS=10 -c $< -o $@

COMMON_OBJS = stub.o sched.o wakeups.o main_task.o preempt_ctx_switch.o

.PHONY: all run clean
all: $(addsuffix .elf,$(TESTS))

%.elf: %.o $(COMMON_OBJS) ../common/linker.ld
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) $(LDFLAGS) -o $@

t05_timerd_cadence.elf: t05_timerd_cadence.o stub.o sched.o wakeups.o main_task.o preempt_ctx_switch_10.o ../common/linker.ld
	$(CC) $(CFLAGS) $< stub.o sched.o wakeups.o main_task.o preempt_ctx_switch_10.o $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	@for t in $(TESTS); do \
		echo "=== pre/$$t ==="; \
		timeout --foreground $(TIMEOUT_SECS) $(QEMU) $(QFLAGS) -kernel $$t.elf 2>/dev/null || true; \
		echo; \
	done

clean:
	rm -f *.o *.elf
