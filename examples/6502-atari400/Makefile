EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build
ATARI400_DIR := $(EXAMPLE_DIR)../../src/rt/atari400
ATARI400_LIB := $(ATARI400_DIR)/atari400.lib

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.atari

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=6502-atari --dump-ir --dump-asm --out="$@" "$<"

# atari400.lib is src/rt/atari400/'s own build product (see that
# directory's Makefile) - built here via a sub-make rather than
# recompiling atari400.c a second time, so this example links the same
# object every other atari400.c consumer would.
$(ATARI400_LIB):
	$(MAKE) -C $(ATARI400_DIR)

# --base=$2000: clear of the frame-storage region M6502Codegen.cpp
# reserves at $0200-$05FF and of the OS's own low-memory work area -
# a reasonable load address for a cartridge-free Atari 400/800 with at
# least 16K of RAM (unverified against every real memory configuration -
# see src/rt/atari400/atari400.h's own header comment).
$(BUILD_DIR)/main.atari: $(BUILD_DIR)/main.o $(ATARI400_LIB)
	$(RCC) --link --platform=6502-atari --base='$$2000' --out="$@" "$(BUILD_DIR)/main.o" "$(ATARI400_LIB)"

# Same link, written out as `raw` instead of `atari` - identical program
# bytes minus the segment headers and RUNAD-poke segment (see ../../src/
# fileformat/atari/AtariExeFormat.h) - so CI can load and run it directly
# in py65 without having to parse the segment format first. Same --base
# as main.atari above - they need to stay identical bytes.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(ATARI400_LIB)
	$(RCC) --link --platform=6502-raw --base='$$2000' --out="$@" "$(BUILD_DIR)/main.o" "$(ATARI400_LIB)"

# Runs main.raw in py65's 6502 MPU core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.raw
	python3 "test.py" "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
