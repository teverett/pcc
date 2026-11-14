EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.a26

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=atari2600-a26 --dump-ir --dump-asm --out="$@" "$<"

# --base=$F000: a 4K cartridge image (see ../../src/fileformat/
# atari2600/Atari2600RomFormat.h) - $F000/$F800 are the only two bases
# --format=a26 accepts.
$(BUILD_DIR)/main.a26: $(BUILD_DIR)/main.o
	$(RCC) --link --platform=atari2600-a26 --base='$$F000' --out="$@" "$(BUILD_DIR)/main.o"

# Same link, written out as `raw` instead of `a26` - identical program
# bytes minus the cartridge padding/vector table - so CI can load and run
# it directly in py65 without having to parse the cartridge format
# first. Same --base as main.a26 above - they need to stay identical
# bytes.
$(BUILD_DIR)/main.raw: $(BUILD_DIR)/main.o
	$(RCC) --link --platform=atari2600-raw --base='$$F000' --out="$@" "$(BUILD_DIR)/main.o"

# Runs main.raw in py65's 6502 MPU core (test.py) - see that script's own
# header comment.
test: $(BUILD_DIR)/main.raw
	python3 "test.py" "$(BUILD_DIR)/main.raw"

clean:
	rm -rf "$(BUILD_DIR)"
