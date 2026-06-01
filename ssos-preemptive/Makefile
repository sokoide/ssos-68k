ifeq ($(shell which m68k-xelf-gcc),)
$(error Cross-compilation toolchain not found. Please run: . ~/.elf2x68k)
endif

TOPTARGETS := all
SUBDIRS := boot os standalone

.PHONY: $(TOPTARGETS) $(SUBDIRS) clean

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

all: boot os standalone

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
