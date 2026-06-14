ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TOPTARGETS := dump readelf
SUBDIRS := boot os
DISK := ssos_cop.xdf
MAKEDISK := /Users/scott/repo/sokoide/ssos-68k/tools/makedisk/makedisk

.PHONY: $(TOPTARGETS) $(SUBDIRS) clean all standalone

all: standalone $(DISK)

standalone:
	$(MAKE) -C standalone standalone

dump readelf: $(SUBDIRS) standalone

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(DISK): boot os
	$(MAKEDISK) ./boot/BOOT.X.bin ./os/SSOS.X.bin $(DISK)
	cp $(DISK) ~/tmp

clean:
	for dir in $(SUBDIRS) standalone; do $(MAKE) -C $$dir clean; done
	rm -rf $(DISK)
