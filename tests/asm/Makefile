# m68k bare-metal assembler tests for QEMU virt machine.
# Mirrors /Users/scott/repo/sokoide/asm/m68k/qemu — same toolchain (m68k-elf-*),
# same machine (-M virt -cpu m68000), Goldfish TTY at 0xff008000.
#
# These are self-contained m68k samples (NOT the SSOS interrupts.s, which is
# X68000-MFP-specific and cannot run on QEMU virt). They teach the primitives
# the SSOS context switch is built on.

SOURCES = t01_hello t02_subroutines t03_ctx_save_restore t04_stack_frame t05_trap_exception

# Per-sample expected output fragments, checked in addition to the generic
# final-OK verdict in run_test.sh (EXPECT env). Pure-assembly samples have no
# C headers, so no -MMD dependency tracking is needed here.
expect_t01_hello            := Hello, m68k!
expect_t02_subroutines      := subroutines:
expect_t03_ctx_save_restore := t03: OK
expect_t04_stack_frame      := t04: OK
expect_t05_trap_exception   := before ISR after

AS      = m68k-elf-as
LD      = m68k-elf-ld
OD      = m68k-elf-objdump
ASFLAGS = -m68000
LDFLAGS = -T linker.ld

QEMU    = qemu-system-m68k
QFLAGS  = -M virt -cpu m68000 -nographic

# Run timeout. Slower hosts can raise this (e.g. TIMEOUT_SECS=10).
TIMEOUT_SECS ?= 1

.PHONY: all run runall dump clean

all:
	@set -e; for name in $(SOURCES); do \
		echo "=== $$name ==="; \
		$(AS) $(ASFLAGS) -al=$$name.lst -o $$name.o $$name.s && \
		$(LD) $(LDFLAGS) -o $$name.elf $$name.o && \
		rm -f $$name.o && \
		echo "  OK"; \
	done

# Run: make run S=t01_hello
run:
	$(AS) $(ASFLAGS) -al=$(S).lst -o $(S).o $(S).s
	$(LD) $(LDFLAGS) -o $(S).elf $(S).o
	rm -f $(S).o
	EXPECT='$(expect_$(S))' sh ../qemu/common/run_test.sh $(TIMEOUT_SECS) $(QEMU) $(QFLAGS) -kernel $(S).elf

# Run all samples (m68k halt is an infinite loop, so timeout is required).
runall: all
	@set -e; for name in $(SOURCES); do \
		expect=""; \
		case $$name in \
			t01_hello) expect='Hello, m68k!';; \
			t02_subroutines) expect='subroutines:';; \
			t03_ctx_save_restore) expect='t03: OK';; \
			t04_stack_frame) expect='t04: OK';; \
			t05_trap_exception) expect='before ISR after';; \
		esac; \
		echo ""; \
		echo "=== $$name ==="; \
		EXPECT="$$expect" sh ../qemu/common/run_test.sh $(TIMEOUT_SECS) $(QEMU) $(QFLAGS) -kernel $$name.elf; \
		echo ""; \
	done

dump:
	$(OD) -d $(S).elf

clean:
	rm -f *.elf *.o *.lst
