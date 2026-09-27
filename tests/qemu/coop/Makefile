# Cooperative QEMU scheduler tests.
# Builds the real shared scheduler and cooperative wakeup policy plus a
# QEMU port of the cooperative context switch (ctx_switch.s) and runs each
# test under qemu-system-m68k -M virt. Tests yield via movem.l/jmp.

CC      = m68k-elf-gcc
ASFLAGS = -m68000 -Wa,--register-prefix-optional -I../../../ssos/os/kernel
CFLAGS  = -m68000 -O2 -g -ffreestanding -nostdlib -fno-builtin -MMD -MP \
          -Wall -Wextra -Wno-unused-parameter \
          -I../common -I../common/include -I../../../ssos/os/kernel -I../../../ssos/os/ipc \
          -I../../../ssos/os/mem
LDFLAGS = -T ../common/linker.ld -nostdlib -Wl,--gc-sections -lgcc

QEMU    = qemu-system-m68k
QFLAGS  = -M virt -cpu m68000 -nographic
TIMEOUT_SECS ?= 2

SSOS   = ../../../ssos/os

# t04_main_task_register exercises the production main-task bootstrap.
# t05/t08/t09/t10 build the shared common tests; t08 defines the cooperative
# tick-pump variant.
TESTS = t01_single_yield t02_round_robin t03_register_save t04_main_task_register \
	t05_ipc_blocking t06_task_return t07_scheduler_wakeup \
	t08_sleep_sr t09_ipc_samepri t10_alloc_stress

# Shared objects
stub.o: ../common/stub.c
	$(CC) $(CFLAGS) -c $< -o $@
sched.o: $(SSOS)/kernel/scheduler.c
	$(CC) $(CFLAGS) -c $< -o $@
wakeups.o: $(SSOS)/kernel/cooperative/wakeups.c
	$(CC) $(CFLAGS) -c $< -o $@
main_task.o: $(SSOS)/kernel/main_task.c
	$(CC) $(CFLAGS) -c $< -o $@
ipc.o: $(SSOS)/ipc/message.c
	$(CC) $(CFLAGS) -c $< -o $@
buddy.o: $(SSOS)/mem/buddy.c
	$(CC) $(CFLAGS) -c $< -o $@
t05_ipc_blocking.o: ../common/t_ipc_blocking.c
	$(CC) $(CFLAGS) -c $< -o $@
t06_task_return.o: ../common/t_task_return.c
	$(CC) $(CFLAGS) -c $< -o $@
t08_sleep_sr.o: ../common/t_sleep_sr.c
	$(CC) $(CFLAGS) -DSS_COOP_TEST -c $< -o $@
t09_ipc_samepri.o: ../common/t_ipc_samepri.c
	$(CC) $(CFLAGS) -c $< -o $@
t10_alloc_stress.o: ../common/t_alloc_stress.c
	$(CC) $(CFLAGS) -c $< -o $@
ctx_switch.o: ctx_switch.s $(SSOS)/kernel/scheduler_abi.h
	$(CC) $(ASFLAGS) -x assembler-with-cpp -c $< -o $@

COMMON_OBJS = stub.o sched.o wakeups.o main_task.o ipc.o buddy.o ctx_switch.o

-include *.d

.PHONY: all run clean
all: $(addsuffix .elf,$(TESTS))

%.elf: %.o $(COMMON_OBJS) ../common/linker.ld
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	@set -e; for t in $(TESTS); do \
		echo "=== coop/$$t ==="; \
		sh ../common/run_test.sh $(TIMEOUT_SECS) $(QEMU) $(QFLAGS) -kernel $$t.elf; \
		echo; \
	done

clean:
	rm -f *.o *.elf *.d
