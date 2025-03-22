TOPTARGETS := all clean
SUBDIRS := makedisk boot os
DISK := ssos.xdf

.PHONY: $(TARGETS) $(SUBDIRS) all compiledb local
.default: all

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

all: $(DISK)
	cp $(DISK) ~/tmp

$(DISK): $(SUBDIRS)
	./makedisk/makedisk ./boot/BOOT.X.bin ./os/SSOS.X.bin $(DISK)

clean:
	cd local; make clean
	rm $(DISK)

compiledb: $(SRCS)
	compiledb make

local:
	cd local; make clean; make

local:
	cd local; make
