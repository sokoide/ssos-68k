TOPTARGETS := all format
SUBDIRS := boot os standalone
DISK := ssos.xdf

.PHONY: $(TOPTARGETS) $(SUBDIRS) clean compiledb format
.default: all

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

all: $(DISK)
	cp $(DISK) ~/tmp

$(DISK): $(SUBDIRS)
	../tools/makedisk/makedisk ./boot/BOOT.X.bin ./os/SSOS.X.bin $(DISK)

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
	rm -rf $(DISK)

compiledb: $(SRCS)
	compiledb make
