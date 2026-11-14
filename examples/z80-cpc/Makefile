EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build
CPC_DIR := $(EXAMPLE_DIR)../../src/rt/cpc
CPC_LIB := $(CPC_DIR)/cpc.lib

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.bin

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=z80-amsdos --dump-ir --dump-asm --out="$@" "$<"

# cpc.lib is src/rt/cpc/'s own build product (see that directory's own
# Makefile) - built here via a sub-make rather than recompiling cpc.c a
# second time, so this example links the same object every other cpc.c
# consumer would.
$(CPC_LIB):
	$(MAKE) -C $(CPC_DIR)

$(BUILD_DIR)/main.bin: $(BUILD_DIR)/main.o $(CPC_LIB)
	$(RCC) --link --platform=z80-amsdos --out="$@" "$(BUILD_DIR)/main.o" "$(CPC_LIB)"

# Same link, written out as `raw` instead of `amsdos` - identical
# program bytes minus the 128-byte AMSDOS header (see ../../src/
# fileformat/amsdos/AmsdosFormat.h) - so CI can load and run it directly
# in a Z80 simulator without having to parse the header first.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o $(CPC_LIB)
	$(RCC) --link --platform=z80-raw --out="$@" "$(BUILD_DIR)/main.o" "$(CPC_LIB)"

# Runs main.raw on a real Z80 core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.raw
	python3 "test.py" "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
