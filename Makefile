TOPTARGETS := all clean
SUBDIRS := makedisk ssos
DISK := ssos.xdf

.PHONY: $(TARGETS) $(SUBDIRS) all
.default: all

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

