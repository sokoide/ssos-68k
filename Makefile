TOPTARGETS := all clean
SUBDIRS := tools/makedisk ssos
DISK := ssos.xdf

MD_FILES := $(shell find . -name "*.md" -not -path "*/.*/*" -not -path "./conductor/*")

.PHONY: $(TOPTARGETS) $(SUBDIRS) all format
.default: all

$(TOPTARGETS): $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

format:
	markdownlint --fix $(MD_FILES)

