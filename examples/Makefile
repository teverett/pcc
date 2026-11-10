EXAMPLES_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

.PHONY: all example1 example2 clean

all: example1 example2

example1:
	$(MAKE) -C $(EXAMPLES_DIR)example1

example2:
	$(MAKE) -C $(EXAMPLES_DIR)example2

clean:
	$(MAKE) -C $(EXAMPLES_DIR)example1 clean
	$(MAKE) -C $(EXAMPLES_DIR)example2 clean
