ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

# Threading model propagated to all sub-builds.
SCHED ?= cooperative
ifeq ($(SCHED),cooperative)
else ifeq ($(SCHED),preemptive)
else
$(error Invalid SCHED='$(SCHED)'. Use: make SCHED=cooperative|preemptive)
endif

# Disk image name reflects the threading model.
ifeq ($(SCHED),cooperative)
DISK := ssos_cop.xdf
else
DISK := ssos_pre.xdf
endif

TOPTARGETS := dump readelf
SUBDIRS := boot os
MAKEDISK := /Users/scott/repo/sokoide/ssos-68k/tools/makedisk/makedisk

.PHONY: $(TOPTARGETS) $(SUBDIRS) clean all standalone

all: standalone $(DISK)

standalone:
	$(MAKE) -C standalone standalone SCHED=$(SCHED)

dump readelf: $(SUBDIRS) standalone

# boot is independent of SCHED (unified boot loader); os needs SCHED.
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS) SCHED=$(SCHED)

$(DISK): boot os
	$(MAKEDISK) ./boot/BOOT.X.bin ./os/SSOS.X.bin $(DISK)
	cp $(DISK) ~/tmp

clean:
	for dir in $(SUBDIRS) standalone; do $(MAKE) -C $$dir clean SCHED=$(SCHED); done
	rm -rf $(DISK)
