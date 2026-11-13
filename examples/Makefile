EXAMPLES_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

.PHONY: all example1 example2 example3 example4 clean

all: example1 example2 example3 example4

example1:
	$(MAKE) -C $(EXAMPLES_DIR)example1

example2:
	$(MAKE) -C $(EXAMPLES_DIR)example2

example3:
	$(MAKE) -C $(EXAMPLES_DIR)example3

example4:
	$(MAKE) -C $(EXAMPLES_DIR)example4

clean:
	$(MAKE) -C $(EXAMPLES_DIR)example1 clean
	$(MAKE) -C $(EXAMPLES_DIR)example2 clean
	$(MAKE) -C $(EXAMPLES_DIR)example3 clean
	$(MAKE) -C $(EXAMPLES_DIR)example4 clean
