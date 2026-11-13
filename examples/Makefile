EXAMPLES_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

.PHONY: all hosted-hello m30626fjpfp-freestanding 6502-freestanding 6502-dos33 clean

all: hosted-hello m30626fjpfp-freestanding 6502-freestanding 6502-dos33

hosted-hello:
	$(MAKE) -C $(EXAMPLES_DIR)hosted-hello

m30626fjpfp-freestanding:
	$(MAKE) -C $(EXAMPLES_DIR)m30626fjpfp-freestanding

6502-freestanding:
	$(MAKE) -C $(EXAMPLES_DIR)6502-freestanding

6502-dos33:
	$(MAKE) -C $(EXAMPLES_DIR)6502-dos33

clean:
	$(MAKE) -C $(EXAMPLES_DIR)hosted-hello clean
	$(MAKE) -C $(EXAMPLES_DIR)m30626fjpfp-freestanding clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-freestanding clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-dos33 clean
