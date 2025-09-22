TOPTARGETS := all clean
SUBDIRS := boot os
DISK := ssos.xdf

.PHONY: $(TARGETS) $(SUBDIRS) all compiledb local
.default: all

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

all: $(DISK)
	cp $(DISK) ~/tmp

$(DISK): $(SUBDIRS)
	../tools/makedisk/makedisk ./boot/BOOT.X.bin ./os/SSOS.X.bin $(DISK)

clean:
	cd local; make clean
	rm -rf $(DISK)

compiledb: $(SRCS)
	compiledb make

local:
	cd local; make
