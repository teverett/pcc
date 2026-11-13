EXAMPLES_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

.PHONY: all hosted-hello m30626fjpfp-freestanding 6502-freestanding 68000-freestanding 6502-dos33 z80-trs80 z80-coleco 6809-coco clean

all: hosted-hello m30626fjpfp-freestanding 6502-freestanding 68000-freestanding 6502-dos33 z80-trs80 z80-coleco 6809-coco

hosted-hello:
	$(MAKE) -C $(EXAMPLES_DIR)hosted-hello

m30626fjpfp-freestanding:
	$(MAKE) -C $(EXAMPLES_DIR)m30626fjpfp-freestanding

6502-freestanding:
	$(MAKE) -C $(EXAMPLES_DIR)6502-freestanding

68000-freestanding:
	$(MAKE) -C $(EXAMPLES_DIR)68000-freestanding

6502-dos33:
	$(MAKE) -C $(EXAMPLES_DIR)6502-dos33

z80-trs80:
	$(MAKE) -C $(EXAMPLES_DIR)z80-trs80

z80-coleco:
	$(MAKE) -C $(EXAMPLES_DIR)z80-coleco

6809-coco:
	$(MAKE) -C $(EXAMPLES_DIR)6809-coco

clean:
	$(MAKE) -C $(EXAMPLES_DIR)hosted-hello clean
	$(MAKE) -C $(EXAMPLES_DIR)m30626fjpfp-freestanding clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-freestanding clean
	$(MAKE) -C $(EXAMPLES_DIR)68000-freestanding clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-dos33 clean
	$(MAKE) -C $(EXAMPLES_DIR)z80-trs80 clean
	$(MAKE) -C $(EXAMPLES_DIR)z80-coleco clean
	$(MAKE) -C $(EXAMPLES_DIR)6809-coco clean
