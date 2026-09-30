# Preemptive QEMU scheduler tests.
# Builds the real shared scheduler and preemptive wakeup policy plus a
# QEMU port of the Timer D ISR path (preempt_ctx_switch.s) and runs each test
# under qemu-system-m68k -M virt. Tests fire `trap #0` to drive preemption via
# the `.resume_interrupted`/`rte` path.

CC      = m68k-elf-gcc
ASFLAGS = -m68000 -Wa,--register-prefix-optional -I../../../ssos/os/kernel
CFLAGS  = -m68000 -DSS_PROFILE_RUNTIME=1 -O2 -g -ffreestanding -nostdlib -fno-builtin -MMD -MP \
          -Wall -Wextra -Wno-unused-parameter \
          -I../common -I../common/include -I../../../ssos/os/kernel -I../../../ssos/os/ipc \
          -I../../../ssos/os/mem
LDFLAGS = -T ../common/linker.ld -nostdlib -Wl,--gc-sections -lgcc

QEMU    = qemu-system-m68k
QFLAGS  = -M virt -cpu m68000 -nographic
TIMEOUT_SECS ?= 2

SSOS   = ../../../ssos/os

# t04_main_task_register exercises the production main-task bootstrap.
# t05_timerd_cadence rebuilds the ISR port with the production 10-tick cadence.
# t08/t09/t10 build the shared common tests (trap-pumped variants).
TESTS = t01_round_robin t02_register_save t03_sleep_wakeup t04_main_task_register \
	t05_timerd_cadence t06_ipc_blocking t07_task_return \
	t08_sleep_sr t09_ipc_samepri t10_alloc_stress t11_create_publication t12_capacity t13_trap_restore_sr t14_work_ui t15_target_probe

# Shared objects
stub.o: ../common/stub.c
	$(CC) $(CFLAGS) -c $< -o $@
sched.o: $(SSOS)/kernel/scheduler.c
	$(CC) $(CFLAGS) -c $< -o $@
wakeups.o: $(SSOS)/kernel/preemptive/wakeups.c
	$(CC) $(CFLAGS) -c $< -o $@
main_task.o: $(SSOS)/kernel/main_task.c
	$(CC) $(CFLAGS) -c $< -o $@
ipc.o: $(SSOS)/ipc/message.c
	$(CC) $(CFLAGS) -c $< -o $@
buddy.o: $(SSOS)/mem/buddy.c
	$(CC) $(CFLAGS) -c $< -o $@
t06_ipc_blocking.o: ../common/t_ipc_blocking.c
	$(CC) $(CFLAGS) -c $< -o $@
t07_task_return.o: ../common/t_task_return.c
	$(CC) $(CFLAGS) -c $< -o $@
t08_sleep_sr.o: ../common/t_sleep_sr.c
	$(CC) $(CFLAGS) -c $< -o $@
t09_ipc_samepri.o: ../common/t_ipc_samepri.c
	$(CC) $(CFLAGS) -c $< -o $@
t10_alloc_stress.o: ../common/t_alloc_stress.c
	$(CC) $(CFLAGS) -c $< -o $@
preempt_ctx_switch.o: preempt_ctx_switch.s $(SSOS)/kernel/scheduler_abi.h $(SSOS)/kernel/context_frame.h
	$(CC) $(ASFLAGS) -x assembler-with-cpp -c $< -o $@
preempt_ctx_switch_10.o: preempt_ctx_switch.s $(SSOS)/kernel/scheduler_abi.h $(SSOS)/kernel/context_frame.h
	$(CC) $(ASFLAGS) -Wa,--defsym,SS_TIMERD_SWITCH_TICKS=10 -x assembler-with-cpp -c $< -o $@

COMMON_OBJS = sched_profile.o stub.o sched.o wakeups.o main_task.o ipc.o buddy.o preempt_ctx_switch.o

-include *.d

.PHONY: all run clean
all: $(addsuffix .elf,$(TESTS))

%.elf: %.o $(COMMON_OBJS) ../common/linker.ld
	$(CC) $(CFLAGS) $< $(COMMON_OBJS) $(LDFLAGS) -o $@

t05_timerd_cadence.elf: t05_timerd_cadence.o stub.o sched.o sched_profile.o wakeups.o main_task.o preempt_ctx_switch_10.o ../common/linker.ld
	$(CC) $(CFLAGS) $< stub.o sched.o sched_profile.o wakeups.o main_task.o preempt_ctx_switch_10.o $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	@set -e; for t in $(TESTS); do \
		echo "=== pre/$$t ==="; \
		SS_TEST_CASE=pre-$$t sh ../common/run_test.sh $(TIMEOUT_SECS) $(QEMU) $(QFLAGS) -kernel $$t.elf; \
		echo; \
	done

clean:
	rm -f *.o *.elf *.d

$(filter-out ctx_switch.o preempt_ctx_switch.o preempt_ctx_switch_10.o,$(COMMON_OBJS)) $(addsuffix .o,$(TESTS)): Makefile

sched_probe.o: $(SSOS)/kernel/scheduler.c Makefile
	$(CC) $(CFLAGS) -DSS_TASK_CREATE_PROBE -c $< -o $@
t11_create_publication.o: ../common/t_create_publication.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@
t11_create_publication.elf: t11_create_publication.o sched_probe.o $(filter-out sched.o,$(COMMON_OBJS)) ../common/linker.ld
	$(CC) $(CFLAGS) $< sched_probe.o $(filter-out sched.o,$(COMMON_OBJS)) $(LDFLAGS) -o $@

.PHONY: FORCE
FORCE:
build/config: FORCE
	@python3 ../../../tools/build-config.py $@ $(CC) $(CFLAGS) $(ASFLAGS) $(LDFLAGS)
$(COMMON_OBJS) sched_probe.o preempt_ctx_switch_10.o $(addsuffix .o,$(TESTS)): build/config

t12_capacity.o: ../common/t_capacity.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

trap14_prod.o: $(SSOS)/../standalone/trap14.s $(SSOS)/../include/iocscall.mac Makefile
	m68k-elf-as -m68000 --register-prefix-optional --traditional-format -I$(SSOS)/../include $< -o $@
work_queue.o: $(SSOS)/kernel/work_queue.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@
t13_trap_restore_sr.o: ../common/t_trap_restore_sr.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@
t14_work_ui.o: ../common/t_work_ui.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@
t13_trap_restore_sr.elf: t13_trap_restore_sr.o trap14_prod.o $(COMMON_OBJS) ../common/linker.ld
	$(CC) $(CFLAGS) $< trap14_prod.o $(COMMON_OBJS) $(LDFLAGS) -o $@
t14_work_ui.elf: t14_work_ui.o work_queue.o $(COMMON_OBJS) ../common/linker.ld
	$(CC) $(CFLAGS) $< work_queue.o $(COMMON_OBJS) $(LDFLAGS) -o $@

target_probe.o: $(SSOS)/kernel/task_probe.c Makefile
	$(CC) $(CFLAGS) -DSS_DIAG_TASKS=32 -c $< -o $@
t15_target_probe.o: ../common/t_target_probe.c Makefile
	$(CC) $(CFLAGS) -DSS_DIAG_TASKS=32 -c $< -o $@
t15_target_probe.elf: t15_target_probe.o target_probe.o $(COMMON_OBJS) ../common/linker.ld
	$(CC) $(CFLAGS) $< target_probe.o $(COMMON_OBJS) $(LDFLAGS) -o $@

sched_profile.o: $(SSOS)/kernel/sched_profile.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@
