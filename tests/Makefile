# Top-level test router.
#
#   make test      -> native C tests (both SCHED variants), CI exit code
#   make test-asm  -> m68k asm samples under QEMU
#   make clean     -> clean both

.PHONY: test test-native test-asm clean

test: test-native

test-native:
	@$(MAKE) -f Makefile.native test SCHED=cooperative
	@$(MAKE) -f Makefile.native test SCHED=preemptive
	@echo ""
	@echo "=== All native tests passed (cooperative + preemptive) ==="

test-asm:
	$(MAKE) -C asm runall

clean:
	$(MAKE) -f Makefile.native clean
	$(MAKE) -C asm clean
