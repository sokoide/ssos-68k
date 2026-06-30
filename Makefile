TOPTARGETS := all clean
SUBDIRS := tools/makedisk

# use pnpm if available, otherwise npx
RUNNER := $(shell command -v pnpm >/dev/null 2>&1 && echo "pnpm dlx" || echo "npx")
EXEC := $(shell command -v pnpm >/dev/null 2>&1 && echo "pnpm exec" || echo "npx")

.PHONY: $(TOPTARGETS) $(SUBDIRS) all format ssos-cooperative ssos-preemptive test test-asm
.default: all

# Single unified tree under ssos/; the threading model is selected by SCHED=.
all: tools/makedisk ssos-cooperative ssos-preemptive

ssos-cooperative:
	$(MAKE) -C ssos all SCHED=cooperative

ssos-preemptive:
	$(MAKE) -C ssos all SCHED=preemptive

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

clean:
	$(MAKE) -C tools/makedisk clean
	$(MAKE) -C ssos clean SCHED=cooperative
	$(MAKE) -C ssos clean SCHED=preemptive

format:
	@echo "Formatting markdown files using $(RUNNER)..."
	$(RUNNER) markdownlint-cli "**/*.md" --ignore "conductor/**" --ignore "CLAUDE.md" --ignore "node_modules/**" --fix
	$(EXEC) textlint --fix "**/*.md"

# Native C tests (both SCHED variants) and QEMU asm samples. See tests/README.md.
test:
	$(MAKE) -C tests test

test-asm:
	$(MAKE) -C tests test-asm
