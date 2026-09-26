ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

# Threading model propagated to all sub-builds.
SCHED ?= cooperative
SS_PROFILE_GFX ?= 0
ifeq ($(SS_PROFILE_GFX),0)
PROFILE_SUFFIX :=
else
PROFILE_SUFFIX := _profile$(SS_PROFILE_GFX)
endif
ifeq ($(SCHED),cooperative)
else ifeq ($(SCHED),preemptive)
else
$(error Invalid SCHED='$(SCHED)'. Use: make SCHED=cooperative|preemptive)
endif

# Disk image name reflects the threading model.
ifeq ($(SCHED),cooperative)
DISK := ssos_cop$(PROFILE_SUFFIX).xdf
else
DISK := ssos_pre$(PROFILE_SUFFIX).xdf
endif

TOPTARGETS := dump readelf
SUBDIRS := boot os
MAKEDISK := ../tools/makedisk/makedisk
BOOT_BIN := boot/build/$(SCHED)/BOOT.X.bin
OS_BIN := os/build/$(SCHED)/profile-$(SS_PROFILE_GFX)/SSOS.X.bin
OS_INPUTS := $(wildcard os/*/*.c os/*/*.s os/*/*.h os/*/*/*.c \
	os/*/*/*.s os/*/*/*.h os/kernel/linker.ld os/Makefile)
DISK_DIR := build/disk/$(SCHED)/profile-$(SS_PROFILE_GFX)
DISK_FILE := $(DISK_DIR)/$(DISK)

.PHONY: $(TOPTARGETS) $(SUBDIRS) clean all standalone

all: standalone $(DISK_FILE)

standalone:
	$(MAKE) -C standalone standalone SCHED=$(SCHED) SS_PROFILE_GFX=$(SS_PROFILE_GFX)

dump readelf: $(SUBDIRS) standalone

# boot is independent of SCHED (unified boot loader); os needs SCHED.
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS) SCHED=$(SCHED)

$(DISK_FILE): $(BOOT_BIN) $(OS_BIN) $(MAKEDISK)
	@mkdir -p $(DISK_DIR)
	$(MAKEDISK) $(BOOT_BIN) $(OS_BIN) $@
	cp $@ $(HOME)/tmp/$(DISK)

$(BOOT_BIN): boot/main.s boot/linker.ld boot/Makefile
	$(MAKE) -C boot all SCHED=$(SCHED)

$(OS_BIN): $(OS_INPUTS)
	$(MAKE) -C os all SCHED=$(SCHED) SS_PROFILE_GFX=$(SS_PROFILE_GFX)

$(MAKEDISK): ../tools/makedisk/main.c ../tools/makedisk/Makefile
	$(MAKE) -C ../tools/makedisk all

clean:
	for dir in $(SUBDIRS) standalone; do $(MAKE) -C $$dir clean SCHED=$(SCHED); done
	rm -rf build
	rm -f $(DISK)
