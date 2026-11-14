EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build
ZXSPECTRUM_DIR := $(EXAMPLE_DIR)../../src/rt/zxspectrum
ZXSPECTRUM_LIB := $(ZXSPECTRUM_DIR)/zxspectrum.lib

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.sna

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=z80-sna --dump-ir --dump-asm --out="$@" "$<"

# zxspectrum.lib is src/rt/zxspectrum/'s own build product (see that
# directory's own Makefile) - built here via a sub-make rather than
# recompiling zxspectrum.c a second time, so this example links the same
# object every other zxspectrum.c consumer would.
$(ZXSPECTRUM_LIB):
	$(MAKE) -C $(ZXSPECTRUM_DIR)

$(BUILD_DIR)/main.sna: $(BUILD_DIR)/main.o $(ZXSPECTRUM_LIB)
	$(RCC) --link --platform=z80-sna --out="$@" "$(BUILD_DIR)/main.o" "$(ZXSPECTRUM_LIB)"

# Same link, written out as `raw` instead of `sna` - identical program
# bytes minus the snapshot's own 27-byte register header and the
# bootstrap-trick framing around the 48K RAM dump (see ../../src/
# fileformat/sna/SnaFormat.h) - so CI can load and run it directly in a
# Z80 simulator without having to parse the snapshot format first.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(ZXSPECTRUM_LIB)
	$(RCC) --link --platform=z80-raw --out="$@" "$(BUILD_DIR)/main.o" "$(ZXSPECTRUM_LIB)"

# Runs main.raw on a real Z80 core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.raw
	python3 "test.py" "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
