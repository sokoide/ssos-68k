# m68k bare-metal assembler tests for QEMU virt machine.
# Mirrors /Users/scott/repo/sokoide/asm/m68k/qemu — same toolchain (m68k-elf-*),
# same machine (-M virt -cpu m68000), Goldfish TTY at 0xff008000.
#
# These are self-contained m68k samples (NOT the SSOS interrupts.s, which is
# X68000-MFP-specific and cannot run on QEMU virt). They teach the primitives
# the SSOS context switch is built on.

SOURCES = t01_ctx_save_restore

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
	@for name in $(SOURCES); do \
		echo "=== $$name ==="; \
		$(AS) $(ASFLAGS) -al=$$name.lst -o $$name.o $$name.s && \
		$(LD) $(LDFLAGS) -o $$name.elf $$name.o && \
		rm -f $$name.o && \
		echo "  OK"; \
	done

# Run: make run S=t01_ctx_save_restore
run:
	$(AS) $(ASFLAGS) -al=$(S).lst -o $(S).o $(S).s
	$(LD) $(LDFLAGS) -o $(S).elf $(S).o
	rm -f $(S).o
	timeout --foreground $(TIMEOUT_SECS) $(QEMU) $(QFLAGS) -kernel $(S).elf || true

# Run all samples (m68k halt is an infinite loop, so timeout is required).
runall: all
	@for name in $(SOURCES); do \
		echo ""; \
		echo "=== $$name ==="; \
		timeout --foreground $(TIMEOUT_SECS) $(QEMU) $(QFLAGS) -kernel $$name.elf 2>/dev/null || true; \
		echo ""; \
	done

dump:
	$(OD) -d $(S).elf

clean:
	rm -f *.elf *.o *.lst
