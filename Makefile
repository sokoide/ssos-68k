TOPTARGETS := all clean
SUBDIRS := tools/makedisk ssos
DISK := ssos.xdf

.PHONY: $(TARGETS) $(SUBDIRS) all
.default: all

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

