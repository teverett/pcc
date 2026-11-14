EXAMPLES_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

.PHONY: all hosted-hello m30626fjpfp-freestanding 6502-freestanding 68000-freestanding pdp7-freestanding 6502-dos33 6502-vic20 6502-c64 6502-atari400 6502-atari2600 6502-2a03 8086-msdos z80-trs80 z80-coleco z80-zxspectrum z80-cpc z80-zx81 6809-coco test clean

all: hosted-hello m30626fjpfp-freestanding 6502-freestanding 68000-freestanding pdp7-freestanding 6502-dos33 6502-vic20 6502-c64 6502-atari400 6502-atari2600 6502-2a03 8086-msdos z80-trs80 z80-coleco z80-zxspectrum z80-cpc z80-zx81 6809-coco

hosted-hello:
	$(MAKE) -C $(EXAMPLES_DIR)hosted-hello

m30626fjpfp-freestanding:
	$(MAKE) -C $(EXAMPLES_DIR)m30626fjpfp-freestanding

6502-freestanding:
	$(MAKE) -C $(EXAMPLES_DIR)6502-freestanding

68000-freestanding:
	$(MAKE) -C $(EXAMPLES_DIR)68000-freestanding

pdp7-freestanding:
	$(MAKE) -C $(EXAMPLES_DIR)pdp7-freestanding

6502-dos33:
	$(MAKE) -C $(EXAMPLES_DIR)6502-dos33

6502-vic20:
	$(MAKE) -C $(EXAMPLES_DIR)6502-vic20

6502-c64:
	$(MAKE) -C $(EXAMPLES_DIR)6502-c64

6502-atari400:
	$(MAKE) -C $(EXAMPLES_DIR)6502-atari400

6502-atari2600:
	$(MAKE) -C $(EXAMPLES_DIR)6502-atari2600

6502-2a03:
	$(MAKE) -C $(EXAMPLES_DIR)6502-2a03

8086-msdos:
	$(MAKE) -C $(EXAMPLES_DIR)8086-msdos

z80-trs80:
	$(MAKE) -C $(EXAMPLES_DIR)z80-trs80

z80-coleco:
	$(MAKE) -C $(EXAMPLES_DIR)z80-coleco

z80-zxspectrum:
	$(MAKE) -C $(EXAMPLES_DIR)z80-zxspectrum

z80-cpc:
	$(MAKE) -C $(EXAMPLES_DIR)z80-cpc

z80-zx81:
	$(MAKE) -C $(EXAMPLES_DIR)z80-zx81

6809-coco:
	$(MAKE) -C $(EXAMPLES_DIR)6809-coco

# Each example's own `test` target builds it, then verifies it in
# whichever simulator (py65, z80-python, qemu-system-m68k) that target
# has one wired up for - see each example's own Makefile/README for which.
test:
	$(MAKE) -C $(EXAMPLES_DIR)hosted-hello test
	$(MAKE) -C $(EXAMPLES_DIR)m30626fjpfp-freestanding test
	$(MAKE) -C $(EXAMPLES_DIR)6502-freestanding test
	$(MAKE) -C $(EXAMPLES_DIR)68000-freestanding test
	$(MAKE) -C $(EXAMPLES_DIR)pdp7-freestanding test
	$(MAKE) -C $(EXAMPLES_DIR)6502-dos33 test
	$(MAKE) -C $(EXAMPLES_DIR)6502-vic20 test
	$(MAKE) -C $(EXAMPLES_DIR)6502-c64 test
	$(MAKE) -C $(EXAMPLES_DIR)6502-atari400 test
	$(MAKE) -C $(EXAMPLES_DIR)6502-atari2600 test
	$(MAKE) -C $(EXAMPLES_DIR)6502-2a03 test
	$(MAKE) -C $(EXAMPLES_DIR)z80-trs80 test
	$(MAKE) -C $(EXAMPLES_DIR)z80-coleco test
	$(MAKE) -C $(EXAMPLES_DIR)z80-zxspectrum test
	$(MAKE) -C $(EXAMPLES_DIR)z80-cpc test
	$(MAKE) -C $(EXAMPLES_DIR)z80-zx81 test
	$(MAKE) -C $(EXAMPLES_DIR)8086-msdos test
	$(MAKE) -C $(EXAMPLES_DIR)6809-coco test

clean:
	$(MAKE) -C $(EXAMPLES_DIR)hosted-hello clean
	$(MAKE) -C $(EXAMPLES_DIR)m30626fjpfp-freestanding clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-freestanding clean
	$(MAKE) -C $(EXAMPLES_DIR)68000-freestanding clean
	$(MAKE) -C $(EXAMPLES_DIR)pdp7-freestanding clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-dos33 clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-vic20 clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-c64 clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-atari400 clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-atari2600 clean
	$(MAKE) -C $(EXAMPLES_DIR)6502-2a03 clean
	$(MAKE) -C $(EXAMPLES_DIR)8086-msdos clean
	$(MAKE) -C $(EXAMPLES_DIR)z80-trs80 clean
	$(MAKE) -C $(EXAMPLES_DIR)z80-coleco clean
	$(MAKE) -C $(EXAMPLES_DIR)z80-zxspectrum clean
	$(MAKE) -C $(EXAMPLES_DIR)z80-cpc clean
	$(MAKE) -C $(EXAMPLES_DIR)z80-zx81 clean
	$(MAKE) -C $(EXAMPLES_DIR)6809-coco clean
