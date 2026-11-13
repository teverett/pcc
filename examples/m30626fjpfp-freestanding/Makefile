EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= $(EXAMPLE_DIR)build

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.bin

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=m30626fjpfp-raw --dump-ir --dump-asm --out="$@" "$<"

$(BUILD_DIR)/main.bin: $(BUILD_DIR)/main.o
	$(RCC) --link --platform=m30626fjpfp-raw --out="$@" "$<"

# No M16C (or other) emulator is wired into this project anywhere - see
# this example's own README - so this only confirms main.bin exists;
# main.c itself has no observable output (no I/O calls at all) to assert
# against anyway.
test: all
	test -s "$(BUILD_DIR)/main.bin"

clean:
	rm -rf "$(BUILD_DIR)"
