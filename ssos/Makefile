TOPTARGETS := all clean
SUBDIRS := makedisk boot os
DISK := ssos.xdf

.PHONY: $(TARGETS) $(SUBDIRS) install compiledb

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

$(DISK): ./makedisk/makedisk ./boot/boot.X.bin ./os/SSOS.X.bin
	./makedisk/makedisk ./boot/BOOT.X.bin ./os/SSOS.X.bin $(DISK)

install: $(DISK)
	cp $(DISK) ~/tmp

clean:
	rm $(DISK)

compiledb: $(SRCS)
	compiledb make
