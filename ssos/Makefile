TOPTARGETS := all clean
SUBDIRS := makedisk boot os
DISK := ssos.xdf

.PHONY: $(TARGETS) $(SUBDIRS) all compiledb
.default: all


$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

all: $(DISK)
	cp $(DISK) ~/tmp

#$(DISK): ./makedisk/makedisk ./boot/boot.X.bin ./os/SSOS.X.bin
$(DISK): $(SUBDIRS)
	./makedisk/makedisk ./boot/BOOT.X.bin ./os/SSOS.X.bin $(DISK)

clean:
	rm $(DISK)

compiledb: $(SRCS)
	compiledb make
