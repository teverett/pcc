# Builds every runtime library under src/rt/ by delegating to each one's own
# Makefile - see apple2/Makefile, msdos/Makefile, trs80/Makefile, coleco/
# Makefile, coco/Makefile, and vic20/Makefile.
SUBDIRS := apple2 msdos trs80 coleco coco vic20

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done
