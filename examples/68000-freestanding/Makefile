EXAMPLE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
BUILD_DIR ?= build

include $(EXAMPLE_DIR)../examples.mk

.PHONY: all test clean

all: $(BUILD_DIR)/main.bin

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(RCC) --platform=68000-raw --dump-ir --dump-asm --out="$@" "$<"

$(BUILD_DIR)/main.bin: $(BUILD_DIR)/main.o
	$(RCC) --link --platform=68000-raw --out="$@" "$<"

# Runs main.bin under qemu-system-m68k (test.py) - see that script's own
# header comment. Needs qemu-system-m68k on PATH.
test: $(BUILD_DIR)/main.bin
	python3 "test.py" "$(BUILD_DIR)/main.bin"

clean:
	rm -rf "$(BUILD_DIR)"
