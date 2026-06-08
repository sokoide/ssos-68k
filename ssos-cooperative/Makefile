ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TOPTARGETS := all dump readelf
SUBDIRS := boot os standalone
DISK := ssos_cop.xdf
MAKEDISK := /Users/scott/repo/sokoide/ssos-68k/tools/makedisk/makedisk

.PHONY: $(TOPTARGETS) $(SUBDIRS) clean

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

all: $(DISK)
	cp $(DISK) ~/tmp

$(DISK): boot os
	$(MAKEDISK) ./boot/BOOT.X.bin ./os/SSOS.X.bin $(DISK)

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
	rm -rf $(DISK)
