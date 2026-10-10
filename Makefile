TOPTARGETS := all clean
SUBDIRS := tools/makedisk
.DEFAULT_GOAL := usage

# use pnpm if available, otherwise npx
RUNNER := $(shell command -v pnpm >/dev/null 2>&1 && echo "pnpm dlx" || echo "npx")
EXEC := $(shell command -v pnpm >/dev/null 2>&1 && echo "pnpm exec" || echo "npx")
ASM_FORMATTER ?= asmfmt

.PHONY: $(TOPTARGETS) $(SUBDIRS) all format ssos-cooperative ssos-preemptive test test-asm test-qemu verify verify-check

usage:
	@echo "SSOS-68K build and test targets:"
	@echo "  make all              Build both scheduler variants"
	@echo "  make test             Run native tests"
	@echo "  make test-asm         Run m68k assembly samples under QEMU"
	@echo "  make test-qemu        Run scheduler tests under QEMU"
	@echo "  make verify           Run automated verification and report hardware checks"
	@echo "  make clean            Remove build outputs"
	@echo "  make format           Format Markdown and assembly sources"

.PHONY: usage

# Single unified tree under ssos/; the threading model is selected by SCHED=.
all: tools/makedisk ssos-cooperative ssos-preemptive

ssos-cooperative: tools/makedisk
	$(MAKE) -C ssos all SCHED=cooperative

ssos-preemptive: tools/makedisk
	$(MAKE) -C ssos all SCHED=preemptive

$(SUBDIRS):
	$(MAKE) -C $@ all

clean:
	$(MAKE) -C tools/makedisk clean
	$(MAKE) -C ssos clean SCHED=cooperative
	$(MAKE) -C ssos clean SCHED=preemptive

format:
	@echo "Formatting markdown files using $(RUNNER)..."
	$(RUNNER) markdownlint-cli "**/*.md" --ignore "CLAUDE.md" --ignore "node_modules/**" --fix
	$(EXEC) textlint --fix "**/*.md"
	@echo "Formatting assembly files using $(ASM_FORMATTER)..."
	@find . -name '*.s' -not -path './.git/*' -print0 | xargs -0 $(ASM_FORMATTER) -w
	@find . -name '*.s' -not -path './.git/*' -print0 | xargs -0 perl -pi -e 's/^(\s*)\/\/(.*)$$/$$1|$$2/'

# Native C tests (both SCHED variants) and QEMU asm samples. See tests/README.md.
test:
	$(MAKE) -C tests test

test-asm:
	$(MAKE) -C tests test-asm

test-qemu:
	$(MAKE) -C tests test-qemu

# Report which build targets need hardware verification for the current change
# set. REF selects the diff target (default: working tree vs HEAD; use HEAD~1
# to analyze the last commit, or main..HEAD for a branch).
verify-check:
	@tools/verify-check.sh $(REF)

# One-shot: run all automated tests, confirm both SCHED variants build, then
# report any targets that still need a manual hardware check.
verify:
	python3 tools/verify-auto.py

test-tools:
	python3 -m unittest discover -s tests/tools -v

.PHONY: test-tools bench-native
bench-native:
	$(MAKE) -C tests -f Makefile.native bench SCHED=cooperative
	$(MAKE) -C tests -f Makefile.native bench SCHED=preemptive
