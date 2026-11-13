# Builds every runtime library under src/rt/ by delegating to each one's own
# Makefile (currently just apple2/ - see apple2/Makefile).
SUBDIRS := apple2

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done
