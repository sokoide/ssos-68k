# Top-level QEMU scheduler test router.
# Builds and runs both the cooperative and preemptive test sets under
# qemu-system-m68k -M virt. See coop/ and pre/ for per-variant Makefiles.

.PHONY: all run run-coop run-pre clean

all: run-coop run-pre

run-coop:
	$(MAKE) -C coop all

run-pre:
	$(MAKE) -C pre all

run: run-coop run-pre
	@echo "=== cooperative ==="
	@$(MAKE) -C coop run
	@echo "=== preemptive ==="
	@$(MAKE) -C pre run

clean:
	$(MAKE) -C coop clean
	$(MAKE) -C pre clean
