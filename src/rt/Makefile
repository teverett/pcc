# Builds every runtime library under src/rt/ by delegating to each one's own
# Makefile - see apple2/Makefile and msdos/Makefile.
SUBDIRS := apple2 msdos

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done
