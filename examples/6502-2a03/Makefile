EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.nes

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=2a03-nes --dump-ir --dump-asm --out="$@" "$<"

# --base=$8000: NROM's own fixed PRG-ROM window (see ../../src/
# fileformat/nes/NesRomFormat.h) - the only base --format=nes accepts.
$(BUILD_DIR)/main.nes: $(BUILD_DIR)/main.o
	$(RCC) --link --platform=2a03-nes --base='$$8000' --out="$@" "$(BUILD_DIR)/main.o"

# Same link, written out as `raw` instead of `nes` - identical program
# bytes minus the iNES header/padding/vector table - so CI can load and
# run it directly in py65 without having to parse the cartridge format
# first. Same --base as main.nes above - they need to stay identical
# bytes.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o
	$(RCC) --link --platform=2a03-raw --base='$$8000' --out="$@" "$(BUILD_DIR)/main.o"

# Runs main.raw in py65's 6502 MPU core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.raw
	python3 "test.py" "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
